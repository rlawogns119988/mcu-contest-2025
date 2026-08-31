/* =========================================================================
 * ATmega328P @ 16MHz  (순수 AVR 레지스터 제어, Arduino 스타일)
 * - UART(블루투스): 9600-8N1 (RXD=PD0, TXD=PD1)  ※ Serial 미사용, 레지스터 직접 제어
 * - 바퀴: PC0..PC3  (Timer2 소프트 PWM ~1.95 kHz)
 * - 초음파: TRIG=PC4, ECHO=PC5  (Timer1으로 ECHO 펄스폭 측정, 0.5us/tick)
 * - 팬(X/Y): PB0..PB3 (Active-LOW: LOW=ON, HIGH=OFF)  ← 기능부는 유지
 *
 * 요구사항:
 *  1) Auto/Manual에서 같은 방향이면 항상 동일 속도(PWM)로 동작
 *  2) 기능부 코드는 손대지 않음 (핀/동작 유지)
 *  3) 스케치 형식: void setup(), void loop()만 사용
 * ========================================================================= */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/interrupt.h>

/* ------------------------- 핀 매핑 ------------------------- */
/* 기능부(물/세제) — Active-HIGH (LOW=OFF, HIGH=ON) */
#define W_ON   PD5
#define W_SUB  PD4
#define C_ON   PD7
#define C_SUB  PD6

/* 기능부(팬: 솔질/송풍) — Active-LOW (LOW=ON, HIGH=OFF) */
#define B_CW   PB0
#define B_CCW  PB1
#define A_CW   PB2
#define A_CCW  PB3

/* 이동부(바퀴) — Active-HIGH (GO/BACK 중 하나만 구동) */
#define L_GO   PC0
#define L_BACK PC1
#define R_GO   PC2
#define R_BACK PC3

/* 초음파 */
#define US_TRIG PC4
#define US_ECHO PC5

/* ------------------------- 전역 변수(이동부 상태) ------------------------- */
/* ISR에서 참조되므로 일부는 volatile */
static volatile uint8_t pwm_phase = 0;                 // 소프트 PWM 위상(0~255)
static volatile uint8_t dutyL = 0, dutyR = 0;          // 좌/우 듀티(0~255)
static volatile int8_t  dirL  = 0, dirR  = 0;          // 좌/우 방향(-1/0/+1)

static volatile char    BT = '0';                      // Bluetooth 수신 버퍼
static bool    auto_mode = false;                      // 자동/수동 모드 플래그
static uint8_t turn_flip = 0;                          // 회피 좌/우 교대 토글

/* ------------------------- 동작 파라미터(공통) ------------------------- */
/* 여기 값만 수정하면 Auto/Manual 양쪽에 동시에 반영됨 */
#define SAFE_CM         5   // 장애물 임계거리[cm] (초과→전진, 이하→회피)
#define CRUISESPD     210   // 전진 속도(공통)
#define BACKSPD       210  // 후진 속도(공통)
#define TURN_OUTER    270   // 회전 시 바깥바퀴 속도(공통)
#define TURN_INNER    50 // 회전 시 안쪽바퀴 속도(공통)
#define BACK_MS       220   // 근접 시 후진 시간[ms]
#define TURN_MS       320   // 회피 회전 시간[ms]
#define LOOP_MS        80   // Auto: 안전 시 전진 유지 간격[ms]

/* ------------------------- UART(하드웨어) ------------------------- */
/* Arduino의 Serial 대신, USART0 레지스터 직접 사용 */
static inline void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
static inline bool uart_rx_ready(){ return (UCSR0A & (1<<RXC0)); }
static inline char uart_rx(){ while(!uart_rx_ready()); return UDR0; }
static inline void uart_print(const char* s){ while(*s) uart_tx(*s++); }

static inline void uart_init_9600(void){
  /* 16MHz / (16*(UBRR+1)) = 9600 → UBRR=103 */
  UBRR0H = 0;
  UBRR0L = 103;
  UCSR0A = 0;
  UCSR0B = (1<<RXEN0) | (1<<TXEN0);       // RX/TX enable
  UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);     // 8N1
}

/* ------------------------- 초음파(HC-SR04 등) ------------------------- */
/* Timer1: clk/8 → 16MHz/8=2MHz → 0.5us/tick.
 * echo_us = ticks*0.5us, distance_cm ≈ ticks/116
 * 간단한 폴링 타임아웃(guard/TCNT1 상한) 포함
 */
static inline void ultrasonic_init(void){
  DDRC  |=  (1<<US_TRIG);   // TRIG 출력
  DDRC  &= ~(1<<US_ECHO);   // ECHO 입력
  PORTC &= ~(1<<US_TRIG);   // TRIG LOW
}

static uint16_t ultrasonic_read_cm(void){
  uint32_t guard = 0;

  // TRIG 10us 펄스
  PORTC &= ~(1<<US_TRIG);
  _delay_us(2);
  PORTC |=  (1<<US_TRIG);
  _delay_us(10);
  PORTC &= ~(1<<US_TRIG);

  // ECHO 상승 에지 대기 (타임아웃 시 0 반환)
  guard = 0;
  while(!(PINC & (1<<US_ECHO))){
    if(++guard > 60000UL) return 0;
  }

  // ECHO HIGH 구간 폭 측정 (0.5us/tick)
  TCCR1A = 0;
  TCCR1B = (1<<CS11);   // clk/8
  TCNT1  = 0;

  while(PINC & (1<<US_ECHO)){
    if (TCNT1 > 60000){ TCCR1B = 0; return 0; } // 타임아웃
  }
  uint16_t ticks = TCNT1;
  TCCR1B = 0;

  return (uint16_t)(ticks / 116);   // cm 환산
}

/* ------------------------- 소프트 PWM (Timer2) ------------------------- */
/* Timer2 Normal mode, prescaler=32 → f ≈ 16MHz/(32*256) ≈ 1.95kHz
 * ISR에서 dir(±1/0)과 duty(0~255)를 비교하여 GO/BACK 라인 중 하나만 PWM.
 * 반대 라인은 항상 LOW로 유지하여 H-브리지 쇼트 방지.
 */
static inline void pwm_init_timer2(void){
  TCCR2A = 0;                              // Normal mode
  TCCR2B = (1<<CS21) | (1<<CS20);          // prescaler 32
  TIMSK2 = (1<<TOIE2);                     // Overflow interrupt enable
}

/* PWM 비교 및 바퀴 포트 토글 */
ISR(TIMER2_OVF_vect){
  uint8_t ph = pwm_phase++;

  // 왼쪽 바퀴
  if (dirL == 0){
    PORTC &= ~((1<<L_GO)|(1<<L_BACK));             // 정지
  } else if (dirL > 0){
    PORTC &= ~(1<<L_BACK);                         // 전진 시 BACK 강제 LOW
    if (ph < dutyL) PORTC |=  (1<<L_GO); else PORTC &= ~(1<<L_GO);
  } else {
    PORTC &= ~(1<<L_GO);                           // 후진 시 GO 강제 LOW
    if (ph < dutyL) PORTC |=  (1<<L_BACK); else PORTC &= ~(1<<L_BACK);
  }

  // 오른쪽 바퀴
  if (dirR == 0){
    PORTC &= ~((1<<R_GO)|(1<<R_BACK));
  } else if (dirR > 0){
    PORTC &= ~(1<<R_BACK);
    if (ph < dutyR) PORTC |=  (1<<R_GO); else PORTC &= ~(1<<R_GO);
  } else {
    PORTC &= ~(1<<R_GO);
    if (ph < dutyR) PORTC |=  (1<<R_BACK); else PORTC &= ~(1<<R_BACK);
  }
}

/* ------------------------- 모터 속도 설정(저수준) ------------------------- */
/* 입력: left/right ∈ [-255..+255]
 *  - 부호: 방향(− 후진, + 전진, 0 정지)
 *  - 절댓값: 듀티(0..255)
 * Auto/Manual이 같은 방향이면 같은 인자를 쓰도록 상위 공통함수에서 강제.
 */
static inline void set_speed(int16_t left, int16_t right){
  if (left  > 255) left  = 255; if (left  < -255) left  = -255;
  if (right > 255) right = 255; if (right < -255) right = -255;

  if (left > 0){ dirL =  1; dutyL = (uint8_t) left; }
  else if (left < 0){ dirL = -1; dutyL = (uint8_t)(-left); }
  else { dirL = 0; dutyL = 0; }

  if (right > 0){ dirR =  1; dutyR = (uint8_t) right; }
  else if (right < 0){ dirR = -1; dutyR = (uint8_t)(-right); }
  else { dirR = 0; dutyR = 0; }
}

/* ------------------------- 공통 모션 프로파일(단일 소스) ------------------------- */
/* 아래 다섯 함수만 통해 모션을 수행 → Auto/Manual “동일 방향=동일 속도” 보장 */
static inline void go_forward(void){   set_speed(+CRUISESPD, +CRUISESPD); }
static inline void go_backward(void){  set_speed(-BACKSPD,   -BACKSPD  ); }
static inline void turn_left(void){   set_speed(+TURN_INNER, +TURN_OUTER); }
static inline void turn_right(void){    set_speed(+TURN_OUTER, +TURN_INNER); }
static inline void stop_all(void){     set_speed(0, 0); }

/* =========================================================================
 *  setup(): 하드웨어 초기화만 수행 (핀/타이머/UART/초음파/인터럽트)
 * ========================================================================= */
void setup(void){
  /* 기능부 핀(그대로 유지) + 이동부 핀(바퀴) 출력 설정 */
  DDRD |= (1<<W_ON)|(1<<W_SUB)|(1<<C_ON)|(1<<C_SUB);     // 기능부(물/세제)
  DDRB |= (1<<B_CW)|(1<<B_CCW)|(1<<A_CW)|(1<<A_CCW);     // 기능부(팬)
  DDRC |= (1<<L_GO)|(1<<L_BACK)|(1<<R_GO)|(1<<R_BACK);   // 이동부(바퀴)
  // PC4/PC5(초음파)는 ultrasonic_init()에서 별도 설정

  /* 초기 출력 상태: 기능부는 OFF 시작 */
  PORTD = 0x00;    // 물/세제 OFF (Active-HIGH 논리)
  PORTB = 0xFF;    // 팬 OFF (Active-LOW: HIGH=OFF)
  // 바퀴는 ISR로 제어되며 set_speed 호출 전까지 정지 상태

  /* 주변장치 초기화 */
  uart_init_9600();            // 블루투스(UART) 9600-8N1
  uart_print("BT Ready\r\n");  // 준비 메세지

  ultrasonic_init();           // 초음파 TRIG/ECHO 핀 및 대기 상태 설정
  pwm_init_timer2();           // 바퀴 PWM 타이머 시작
  sei();                       // 전역 인터럽트 허용
}

/* =========================================================================
 *  loop(): 
 *   1) 블루투스 명령 처리(Manual 제어/모드 전환)
 *   2) Auto 모드 시 초음파 기반 회피 시퀀스 (블로킹)
 *  - 이동부는 반드시 공통 모션 함수만 호출하여 속도 일관성 보장
 * ========================================================================= */
void loop(void){
  /* 1) 블루투스 수신 처리 (명령: g/d/l/r/s, A, 그리고 기능부 제어 w/c/b/a/f) */
  if (uart_rx_ready()){
    BT = uart_rx();
    uart_tx(BT);
    uart_print(" ");

    switch (BT){
      /* 기능부 제어(유지) */
      case 'w': PORTD |= (1<<W_ON); break;     // water ON
      case 'c': PORTD |= (1<<C_ON); break;     // cleaner ON
      case 'b': PORTB &= ~(1<<B_CW); break;    // brush ON  (Active-LOW)
      case 'a': PORTB &= ~(1<<A_CW); break;    // airfan ON (Active-LOW)
      case 'f': PORTD = 0x00; PORTB = 0xFF; break; // 기능부 모두 OFF

      /* 수동 주행 — 공통 모션 함수만 호출(=Auto와 완전 동일 속도/방향) */
      case 'g': auto_mode=false; go_forward();   break;  // 전진
      case 'd': auto_mode=false; go_backward();  break;  // 후진
      case 'l': auto_mode=false; turn_left();    break;  // 좌회전(제자리/소회전)
      case 'r': auto_mode=false; turn_right();   break;  // 우회전(제자리/소회전)
      case 's': auto_mode=false; stop_all();     break;  // 정지

      /* 모드 전환 */
      case 'A': auto_mode=true;  uart_print("AUTO\r\n");             break;
      default: break; // 정의되지 않은 문자는 무시
    }
  }

  /* 2) Auto 모드: 초음파 기반 회피 (블로킹 시퀀스)
   *  - 전진(안전) / 후진→회피→전진(근접) 로직
   *  - 이동부 모션은 모두 공통 함수만 사용 → Manual과 동일 속도 보장
   */
  if (auto_mode){
    uint16_t cm = ultrasonic_read_cm();

    if (cm == 0){
      // 타임아웃/에러: 잠시 정지 후 재시도
      stop_all();
      _delay_ms(LOOP_MS);
      return;
    }

    if (cm > SAFE_CM){
      // 안전거리 확보 → 전진 유지
      go_forward();
      _delay_ms(LOOP_MS);
    } else {
      // 근접 → 짧게 후진
      go_backward();
      _delay_ms(BACK_MS);

      // 좌/우 교대 회피 (토글)
      if ((turn_flip++ & 1) == 0){
        turn_left();
      } else {
        turn_right();
      }
      _delay_ms(TURN_MS);

      // 전진 재개
      go_forward();
      _delay_ms(LOOP_MS);
    }
  } else {
    // 수동 모드: 폴링 루프 과점유 방지
    _delay_ms(5);
  }
}
