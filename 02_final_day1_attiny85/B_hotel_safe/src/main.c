/* ATtiny85 + TM1650 Smart Lock (Clean Version) */

#define F_CPU 1000000UL // 1MHz 내부 클럭
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

// 핀 설정
#define SDA PB0
#define SCL PB2
#define ALARM_PIN PB3

// TM1650 명령 및 상수
#define TM1650_DISP_CMD 0x48
#define TM1650_KEY_CMD  0x4F
#define TM1650_DIG_BASE 0x68
#define PASS_LEN 4
#define TIMEOUT_MS 10000
#define FEEDBACK_MS 2000

// 전역 변수
uint8_t input_buffer[4];
uint8_t input_len = 0;
uint8_t password[4] = {0, 0, 0, 0};
uint8_t error_cnt = 0;
uint16_t idle_timer = 0;
uint16_t feedback_timer = 0;
uint8_t feedback_type = 0; // 1:SUC, 2:CHA, 3:EEE, 4:EEE5
uint16_t tone_timer = 0;
uint8_t tone_id = 0;

// 세그먼트 폰트 (0~9, E, U, C, 공백, H, A, S, -, 8)
const uint8_t seg_map[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
	0x79, 0x3E, 0x39, 0x00, 0x76, 0x77, 0x6D, 0x40, 0x7F
};

// --- I2C 통신 함수 ---
void I2C_delay() { _delay_us(10); }

void I2C_start() {
	DDRB &= ~((1<<SDA)|(1<<SCL)); PORTB |= (1<<SDA) | (1<<SCL); I2C_delay();
	DDRB |= (1<<SDA); PORTB &= ~(1<<SDA); I2C_delay();
	DDRB |= (1<<SCL); PORTB &= ~(1<<SCL);
}

void I2C_stop() {
	DDRB |= (1<<SDA); PORTB &= ~(1<<SDA); I2C_delay();
	DDRB &= ~(1<<SCL); PORTB |= (1<<SCL); I2C_delay();
	DDRB &= ~(1<<SDA); PORTB |= (1<<SDA); I2C_delay();
}

void I2C_write(uint8_t b) {
	for(int i=0; i<8; i++){
		if(b & 0x80) { DDRB &= ~(1<<SDA); PORTB |= (1<<SDA); }
		else { DDRB |= (1<<SDA); PORTB &= ~(1<<SDA); }
		b <<= 1; I2C_delay();
		DDRB &= ~(1<<SCL); PORTB |= (1<<SCL); I2C_delay();
		DDRB |= (1<<SCL); PORTB &= ~(1<<SCL); I2C_delay();
	}
	DDRB &= ~(1<<SDA); PORTB |= (1<<SDA); I2C_delay();
	DDRB &= ~(1<<SCL); PORTB |= (1<<SCL); I2C_delay();
	DDRB |= (1<<SCL); PORTB &= ~(1<<SCL); I2C_delay();
	DDRB |= (1<<SDA);
}

uint8_t I2C_read() {
	uint8_t data = 0;
	DDRB &= ~(1<<SDA); PORTB |= (1<<SDA);
	for(int i=0; i<8; i++){
		I2C_delay();
		DDRB &= ~(1<<SCL); PORTB |= (1<<SCL); I2C_delay();
		data <<= 1;
		if(PINB & (1<<SDA)) data |= 1;
		DDRB |= (1<<SCL); PORTB &= ~(1<<SCL);
	}
	DDRB &= ~(1<<SDA); PORTB |= (1<<SDA); I2C_delay();
	DDRB &= ~(1<<SCL); PORTB |= (1<<SCL); I2C_delay();
	DDRB |= (1<<SCL); PORTB &= ~(1<<SCL); I2C_delay();
	DDRB |= (1<<SDA);
	return data;
}

// --- TM1650 제어 ---
void TM1650_setConfig() {
	I2C_start(); I2C_write(TM1650_DISP_CMD); I2C_write(0x31); I2C_stop();
}

void TM1650_displayRaw(uint8_t pos, uint8_t data) {
	I2C_start(); I2C_write(TM1650_DIG_BASE + (pos << 1)); I2C_write(data); I2C_stop();
}

uint8_t TM1650_readKey() {
	I2C_start(); I2C_write(TM1650_KEY_CMD); uint8_t key = I2C_read(); I2C_stop();
	return key;
}

uint8_t mapKey(uint8_t k) {
	switch(k) {
		case 0x44: return 1;  case 0x4C: return 2;  case 0x54: return 3;  case 0x5C: return 4;
		case 0x45: return 5;  case 0x4D: return 6;  case 0x55: return 7;  case 0x5D: return 8;
		case 0x46: return 9;  case 0x4E: return 10; case 0x56: return 11; case 0x5E: return 12;
		case 0x47: return 13; case 0x4F: return 14; case 0x57: return 15; case 0x5F: return 16;
		default: return 0;
	}
}

// --- 소리 및 타이밍 제어 ---
void setTone(uint16_t freq_hz, uint16_t duration_ms) {
	if (freq_hz == 0) {
		tone_timer = 0;
		PORTB &= ~(1 << ALARM_PIN);
		return;
	}
	if (freq_hz == 2000) tone_id = 1;      // 2kHz
	else if (freq_hz == 1000) tone_id = 2; // 1kHz
	else if (freq_hz == 500) tone_id = 3;  // 500Hz
	else tone_id = 0;

	tone_timer = duration_ms / 20;
	if (tone_timer == 0) tone_timer = 1;
}

// [핵심] 딜레이를 주면서 소리를 출력 (Loop Delay 대체)
void handle_sound_and_delay(uint8_t tick_ms) {
	uint8_t current_tone_id = 0;
	
	// 소리 출력 여부 결정
	if (tone_timer > 0) current_tone_id = tone_id;
	else if (feedback_type == 4 && (feedback_timer % 25) > 12) current_tone_id = 2;
	else {
		// 소리가 없을 땐 단순 딜레이 (컴파일 오류 수정됨)
		PORTB &= ~(1 << ALARM_PIN);
		for(uint8_t i = 0; i < tick_ms; i++) _delay_ms(1);
		return;
	}

	uint16_t pulse_cycles_us = 0;
	switch (current_tone_id) {
		case 1: pulse_cycles_us = 500; break;  // 2kHz
		case 2: pulse_cycles_us = 1000; break; // 1kHz
		case 3: pulse_cycles_us = 2000; break; // 500Hz
	}
	
	if (pulse_cycles_us == 0) {
		PORTB &= ~(1 << ALARM_PIN);
		for(uint8_t i = 0; i < tick_ms; i++) _delay_ms(1);
		return;
	}

	// PWM 펄스 생성
	uint16_t total_cycles = 1000;
	for(int i=0; i<tick_ms; i++) {
		for (uint16_t j = 0; j < total_cycles; j += pulse_cycles_us) {
			switch (current_tone_id) {
				case 1: PORTB |= (1 << ALARM_PIN); _delay_us(250); PORTB &= ~(1 << ALARM_PIN); _delay_us(250); break;
				case 2: PORTB |= (1 << ALARM_PIN); _delay_us(500); PORTB &= ~(1 << ALARM_PIN); _delay_us(500); break;
				case 3: PORTB |= (1 << ALARM_PIN); _delay_us(1000); PORTB &= ~(1 << ALARM_PIN); _delay_us(1000); break;
				default: _delay_us(1); break;
			}
		}
	}
}

// --- 디스플레이 갱신 ---
void refreshDisplay() {
	if (feedback_timer > 0) {
		// 피드백 메시지 표시
		if (feedback_type == 4) { // EEE5 깜빡임
			if ((feedback_timer % 25) > 12) {
				TM1650_displayRaw(0, seg_map[10]); TM1650_displayRaw(1, seg_map[10]);
				TM1650_displayRaw(2, seg_map[10]); TM1650_displayRaw(3, seg_map[5]);
				} else {
				TM1650_displayRaw(0, 0); TM1650_displayRaw(1, 0); TM1650_displayRaw(2, 0); TM1650_displayRaw(3, 0);
			}
			} else { // SUC, CHA, EEE
			uint8_t msg[4];
			if (feedback_type == 1) { msg[0]=16; msg[1]=11; msg[2]=12; msg[3]=13; } // SUC
			else if (feedback_type == 2) { msg[0]=12; msg[1]=14; msg[2]=15; msg[3]=13; } // CHA
			else { msg[0]=10; msg[1]=10; msg[2]=10; msg[3]=error_cnt%10; } // EEE
			
			for(int i=0; i<4; i++) TM1650_displayRaw(i, seg_map[msg[i]]);
		}
		} else {
		// 입력 상태 표시
		uint8_t disp[4];
		for(int i=0; i<4; i++) disp[i] = seg_map[17]; // ----
		for(int i=0; i<input_len; i++) if(i<4) disp[i] = seg_map[input_buffer[i]];
		for(int i=0; i<4; i++) TM1650_displayRaw(i, disp[i]);
	}
	TM1650_setConfig();
}

void setFeedback(uint8_t type) {
	feedback_type = type;
	feedback_timer = FEEDBACK_MS / 20;
	input_len = 0;
	memset(input_buffer, 0, 4);
}

// ================= MAIN =================
int main(void) {
	// 초기화
	DDRB |= (1 << ALARM_PIN); PORTB &= ~(1 << ALARM_PIN);
	DDRB |= (1<<SDA) | (1<<SCL); PORTB |= (1<<SDA) | (1<<SCL);
	_delay_ms(200);
	TM1650_setConfig();
	refreshDisplay();

	uint8_t last_key = 0;

	while(1) {
		// 1. 루프 딜레이 및 소리 처리 (20ms)
		handle_sound_and_delay(20);

		// 2. 타이머 관리
		if (feedback_timer > 0) feedback_timer--;
		if (idle_timer > 0) idle_timer--;
		if (tone_timer > 0) tone_timer--;

		// 3. 디스플레이 갱신
		refreshDisplay();

		// 4. 키 입력 (간단한 디바운스)
		uint8_t k1 = TM1650_readKey();
		_delay_us(5);
		uint8_t k2 = TM1650_readKey();
		uint8_t key = (k1 == k2) ? mapKey(k1) : 0;

		// 5. 키 처리 로직
		if (key != 0 && key != last_key) {
			
			if (feedback_timer > 0) { // 메시지 표시 중 입력 시 즉시 해제
				feedback_timer = 0;
				setTone(0, 0);
			}
			idle_timer = TIMEOUT_MS / 25;

			if (key >= 1 && key <= 10) { // 숫자 입력
				if (input_len < PASS_LEN) input_buffer[input_len++] = (key==10) ? 0 : key;
			}
			else if (key == 11) { // 삭제
				if (input_len > 0) input_len--;
			}
			else if (key == 12) { // 확인
				if (input_len == PASS_LEN) {
					if (memcmp(input_buffer, password, PASS_LEN) == 0) {
						error_cnt = 0;
						setTone(2000, 150); setFeedback(1); // SUC
						} else {
						error_cnt++;
						if (error_cnt >= 5) {
							setTone(0, 0); setFeedback(4); // EEE5 (락)
							error_cnt = 0;
							} else {
							setTone(500, 150); setFeedback(3); // EEE
						}
					}
				}
			}
			else if (key == 13) { // 비번 변경
				if (input_len == PASS_LEN) {
					memcpy(password, input_buffer, PASS_LEN);
					setTone(1000, 150); setFeedback(2); // CHA
					error_cnt = 0;
				}
			}
		}

		// 6. 타임아웃 초기화
		if (feedback_timer == 0 && input_len > 0 && idle_timer == 0) input_len = 0;

		last_key = key;
	}
}