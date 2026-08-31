/*
 * ATtiny85 Final Version (With Buzzer & Hybrid Keys)
 * [핀 맵]
 * PB0, PB2: TM1650
 * PB1: Mode 버튼 (Digital) -> GND
 * PB4: 피에조 부저 (+)      -> GND
 * PB3: 3-Button ADC Keypad (10k Pull-up 필수!)
 * |-- BTN 1 (Time Set): 직결 (0옴)
 * |-- BTN 3 (Pause)   : 2.2k옴
 * |-- BTN 2 (Start)   : 10k옴
 */

#define F_CPU 1000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

// 핀 정의
#define SDA PB0
#define SCL PB2
#define BTN_MODE_PIN  PB1 // Digital Input (Mode)
#define BUZZER_PIN    PB4 // Output (Buzzer)
#define ADC_PIN       PB3 // Analog Input (BTN 1,2,3)

// TM1650
#define TM1650_CMD  0x48
#define TM1650_DIG0 0x68
#define DOT_ON 0x80
const uint8_t seg_CA[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

volatile uint32_t stopwatch_time = 0;
volatile uint32_t bomb_time = 600; 
volatile uint8_t tick_10ms = 0;
volatile bool is_running = false;
volatile uint8_t current_mode = 0; // 0:Stopwatch, 1:Bomb
volatile bool bomb_exploded = false;

// I2C 함수
void I2C_start() { PORTB |= (1<<SDA)|(1<<SCL); _delay_us(3); PORTB &= ~(1<<SDA); _delay_us(3); PORTB &= ~(1<<SCL); }
void I2C_stop() { PORTB &= ~(1<<SDA); PORTB |= (1<<SCL); _delay_us(3); PORTB |= (1<<SDA); _delay_us(3); }
void I2C_write(uint8_t b) {
    for(int i=0;i<8;i++){
        if(b & 0x80) PORTB |= (1<<SDA); else PORTB &= ~(1<<SDA);
        b <<= 1; PORTB |= (1<<SCL); _delay_us(3); PORTB &= ~(1<<SCL); _delay_us(3);
    }
    PORTB |= (1<<SDA); DDRB &= ~(1<<SDA); PORTB |= (1<<SCL); _delay_us(3); PORTB &= ~(1<<SCL); DDRB |= (1<<SDA);
}
void TM1650_set(int on, int bright) { I2C_start(); I2C_write(TM1650_CMD); I2C_write((bright<<4)|(on?1:0)); I2C_stop(); }
void TM1650_show(int pos, uint8_t seg) { I2C_start(); I2C_write(TM1650_DIG0+(pos<<1)); I2C_write(seg); I2C_stop(); }

void display_time(uint32_t count) {
    uint8_t tenth = count % 10;
    uint8_t sec_unit = (count / 10) % 10;
    uint8_t sec_ten = (count / 100) % 6;
    uint8_t min_unit = (count / 600) % 10;
    TM1650_show(0, seg_CA[min_unit]);
    TM1650_show(1, seg_CA[sec_ten]);
    TM1650_show(2, seg_CA[sec_unit] | DOT_ON);
    TM1650_show(3, seg_CA[tenth]); 
}

// 부저 소리 함수
void beep(uint8_t len) {
    for(int i=0; i<len; i++) {
        PORTB |= (1<<BUZZER_PIN); _delay_us(200);
        PORTB &= ~(1<<BUZZER_PIN); _delay_us(200);
    }
}

void display_boom() {
    for(int i=0; i<4; i++) TM1650_show(i, 0x00);
    beep(100); // 삑!
    for(int i=0; i<4; i++) TM1650_show(i, seg_CA[0]);
    _delay_ms(100);
}

void adc_init() {
    ADMUX = (1 << MUX1) | (1 << MUX0); 
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0); 
}

uint16_t read_adc() {
    ADCSRA |= (1 << ADSC); 
    while (ADCSRA & (1 << ADSC)); 
    return ADC;
}

// ⭐ 버튼 확인 함수 (수정됨) ⭐
uint8_t check_buttons() {
    // 1. 디지털 핀 (BTN 4: Mode)
    if ( !(PINB & (1<<BTN_MODE_PIN)) ) return 4; 

    // 2. 아날로그 핀 (BTN 1, 2, 3)
    uint16_t val = read_adc();
    
    if (val > 950) return 0; // 안 누름

    // BTN 1: 직결 (0V -> 0)
    if (val < 50) return 1;  
    
    // BTN 3: 2.2k옴 (0.9V -> ~184)
    // 범위: 100 ~ 300
    if (val >= 100 && val < 300) return 3;

    // BTN 2: 10k옴 (2.5V -> ~512)
    // 범위: 400 ~ 700
    if (val >= 400 && val < 700) return 2;

    return 0;
}

ISR(TIMER1_COMPA_vect) {
    if (is_running && !bomb_exploded) {
        tick_10ms++;
        if (tick_10ms >= 10) { 
            tick_10ms = 0;
            if (current_mode == 0) {
                stopwatch_time++;
                if (stopwatch_time >= 6000) stopwatch_time = 0; 
            } else {
                if (bomb_time > 0) {
                    bomb_time--;
                } else {
                    is_running = false;
                    bomb_exploded = true;
                }
            }
        }
    }
}

void init_timer1() {
    TCCR1 = 0; TCNT1 = 0;
    TCCR1 |= (1 << CTC1); 
    TCCR1 |= (1 << CS12) | (1 << CS11) | (1 << CS10);
    OCR1C = 156; OCR1A = 156; 
    TIMSK |= (1 << OCIE1A);
    sei();
}

int main(void)
{
    // I2C 출력
    DDRB |= (1<<SDA)|(1<<SCL); PORTB |= (1<<SDA)|(1<<SCL);

    // 부저 출력 설정
    DDRB |= (1<<BUZZER_PIN);

    // Mode 버튼 입력 & 풀업
    DDRB &= ~(1<<BTN_MODE_PIN); PORTB |= (1<<BTN_MODE_PIN);

    _delay_ms(100); TM1650_set(1, 5);
    init_timer1();
    adc_init(); 

    uint32_t last_disp = 99999; 

    while(1){
        // [폭발!]
        if (bomb_exploded) {
            display_boom(); // 소리와 함께 깜빡임
            
            // BTN 3 (2.2k옴) 누르면 해제
            if (check_buttons() == 3) {
                _delay_ms(50);
                if (check_buttons() == 3) {
                    bomb_exploded = false;
                    bomb_time = 600; 
                    display_time(bomb_time);
                    while(check_buttons() != 0); 
                }
            }
            continue;
        }

        uint8_t btn = check_buttons();

        if (btn != 0) { 
            _delay_ms(30); // 디바운싱
            if (check_buttons() == btn) { 
                
                // [스톱워치 모드]
                if (current_mode == 0) { 
                    if (btn == 1) is_running = true;       // Start
                    else if (btn == 2) is_running = false; // Stop
                    else if (btn == 3) {                   // Reset
                        stopwatch_time = 0; is_running = false;
                    }
                }
                
                // [시한폭탄 모드]
                else { 
                    // BTN 1: Time Set (+10초)
                    if (btn == 1) {
                        if (!is_running) {
                            bomb_time += 100; 
                            if (bomb_time > 6000) bomb_time = 100;
                        }
                    }
                    // BTN 2: Start
                    else if (btn == 2) {
                        is_running = true;
                    }
                    // BTN 3: Pause/Resume
                    else if (btn == 3) {
                        is_running = !is_running;
                    }
                }

                // [BTN 4: MODE 변경]
                if (btn == 4) {
                    is_running = false;
                    current_mode = !current_mode; 
                    stopwatch_time = 0;
                    bomb_time = 600;
                }

                while(check_buttons() != 0); 
            }
        }

        uint32_t target = (current_mode == 0) ? stopwatch_time : bomb_time;
        if (target != last_disp) {
            display_time(target);
            last_disp = target;
        }
        _delay_ms(10); 
    }
}