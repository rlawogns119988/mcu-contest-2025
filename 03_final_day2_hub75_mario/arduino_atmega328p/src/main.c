#define F_CPU 16000000UL // 16MHz
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// --- 1. 하드웨어 핀 정의 ---
// ============================================================================
// [데이터 핀] PORTD (D2 ~ D7)
#define BIT_R1 2
#define BIT_G1 3
#define BIT_B1 4
#define BIT_R2 5
#define BIT_G2 6
#define BIT_B2 7

// [행 선택 핀] PORTB (D8 ~ D11)
// A=D8, B=D9, C=D10, D=D11

// [제어 핀] PORTC (A0 ~ A3)
#define CLK_PIN 0  // A0
#define LAT_PIN 1  // A1
#define OE_PIN  2  // A2
#define BUZZER_PIN 3 // A3

// [TM1650] PORTC (A4, A5)
#define TM_SDA_PIN 4 // A4
#define TM_SCL_PIN 5 // A5

// 매크로
#define CTRL_PORT PORTC
#define ROW_PORT  PORTB

// 화면 밝기 (조절 가능)
#define BCM_BASE_DELAY 10

// ============================================================================
// --- 2. 게임 상수 (64x64 화면 대응) ---
// ============================================================================
#define GROUND_Y 54
#define SCREEN_SPLIT_Y 32
#define MARIO_W 12
#define MARIO_H 16
#define GOOMBA_W 12
#define GOOMBA_H 12
#define BOX_W 12
#define BOX_H 12
#define SQUASH_H 6
#define JUMP_LEN 16
#define DEBRIS_SIZE 4
#define COIN_W 5
#define COIN_H 7

#define MAX_GOOMBAS 6
#define MAX_BOXES 3
#define MAX_DEBRIS 8

#define ABS(x) ((x) > 0 ? (x) : -(x))

const int8_t jumpArc[] = { -4, -8, -12, -15, -18, -20, -21, -22, -22, -21, -20, -18, -15, -12, -8, -4 };

// 그래픽 리소스 (PROGMEM)
const uint16_t MarioR[2][MARIO_H] PROGMEM = {{0x0F8,0x1FC,0x1C6,0x1DF,0x1DF,0x1C6,0x0F8,0x1FC,0x3FE,0x3FE,0x3FE,0x30E,0x000,0x0F8,0x1FC,0x000},{0x0F8,0x1FC,0x1C6,0x1DF,0x1DF,0x1C6,0x0F8,0x1FC,0x3FE,0x3FE,0x32E,0x30E,0x18C,0x39C,0x73E,0x600}};
const uint16_t MarioG[2][MARIO_H] PROGMEM = {{0x000,0x000,0x1C6,0x1DF,0x1DF,0x1C6,0x000,0x000,0x0D8,0x0D8,0x000,0x000,0x000,0x0F8,0x1FC,0x000},{0x000,0x000,0x1C6,0x1DF,0x1DF,0x1C6,0x000,0x000,0x0D8,0x0D8,0x020,0x000,0x000,0x39C,0x73E,0x600}};
const uint16_t MarioB[2][MARIO_H] PROGMEM = {{0x000,0x000,0x006,0x01F,0x01F,0x006,0x000,0x020,0x0D8,0x0FC,0x0FC,0x078,0x0FC,0x000,0x000,0x000},{0x000,0x000,0x006,0x01F,0x01F,0x006,0x000,0x020,0x0D8,0x0FC,0x0DC,0x078,0x1FC,0x000,0x000,0x000}};
const uint16_t GoombaR[GOOMBA_H] PROGMEM = { 0x03C0, 0x07E0, 0x0FF0, 0x1FF8, 0x399C, 0x3FFC, 0x1FF8, 0x0FF0, 0x05A0, 0x0DB0, 0x0C30, 0x0000 };
const uint16_t GoombaG[GOOMBA_H] PROGMEM = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0660, 0x0660, 0x03C0, 0x0240, 0x0000, 0x0000, 0x0000 };
const uint16_t GoombaB[GOOMBA_H] PROGMEM = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0660, 0x0660, 0x03C0, 0x0000, 0x0000, 0x0000, 0x0000 };
const uint16_t SquashR[SQUASH_H] PROGMEM = { 0x07E0, 0x1FF8, 0x399C, 0x3FFC, 0x1FF8, 0x0000 };
const uint16_t SquashG[SQUASH_H] PROGMEM = { 0x0000, 0x0000, 0x0660, 0x0660, 0x0000, 0x0000 };
const uint16_t BoxR[BOX_H] PROGMEM = { 0x0FFF, 0x0FFF, 0x0801, 0x0BE1, 0x0BF1, 0x0831, 0x0861, 0x08C1, 0x0801, 0x08C1, 0x0FFF, 0x0FFF };
const uint16_t BoxG[BOX_H] PROGMEM = { 0x0FFF, 0x0FFF, 0x0801, 0x0BE1, 0x0BF1, 0x0831, 0x0861, 0x08C1, 0x0801, 0x08C1, 0x0FFF, 0x0FFF };
const uint16_t BoxB[BOX_H] PROGMEM = { 0x0000, 0x0000, 0x0000, 0x03E0, 0x03F0, 0x0030, 0x0060, 0x00C0, 0x0000, 0x00C0, 0x0000, 0x0000 };
const uint16_t BoxHitR[BOX_H] PROGMEM = { 0x0FFF,0x0801,0x0801,0x0BE1,0x0BF1,0x0831, 0x0861,0x08C1,0x0801,0x08C1,0x0801,0x0FFF };
const uint16_t BoxHitG[BOX_H] PROGMEM = { 0x0888,0x0000,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x0000,0x0000,0x0000,0x0888 };
const uint16_t BoxHitB[BOX_H] PROGMEM = { 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 };
const uint8_t CoinR[COIN_H] PROGMEM = { 0x0E, 0x11, 0x15, 0x15, 0x15, 0x11, 0x0E };
const uint8_t HeartIcon[5] PROGMEM = { 0b01010, 0b11111, 0b11111, 0b01110, 0b00100 };
const uint8_t NumFont[11][5] PROGMEM = {{0x02, 0x05, 0x05, 0x05, 0x02}, {0x02, 0x02, 0x02, 0x02, 0x02}, {0x07, 0x01, 0x07, 0x04, 0x07},{0x07, 0x01, 0x03, 0x01, 0x07}, {0x05, 0x05, 0x07, 0x01, 0x01}, {0x07, 0x04, 0x07, 0x01, 0x07},{0x07, 0x04, 0x07, 0x05, 0x07}, {0x07, 0x01, 0x01, 0x02, 0x02}, {0x07, 0x05, 0x02, 0x05, 0x07},{0x07, 0x05, 0x07, 0x01, 0x07}, {0x00, 0x02, 0x01, 0x02, 0x00}};
const uint8_t BigFont_P[7] PROGMEM = {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40};
const uint8_t BigFont_U[7] PROGMEM = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C};
const uint8_t BigFont_S[7] PROGMEM = {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x42, 0x3C};
const uint8_t BigFont_H[7] PROGMEM = {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42};
const uint8_t BigFont_G[7] PROGMEM = {0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C};
const uint8_t BigFont_A[7] PROGMEM = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42};
const uint8_t BigFont_M[7] PROGMEM = {0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x42};
const uint8_t BigFont_E[7] PROGMEM = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E};
const uint8_t BigFont_O[7] PROGMEM = {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C};
const uint8_t BigFont_V[7] PROGMEM = {0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x08};
const uint8_t BigFont_R[7] PROGMEM = {0x7C, 0x42, 0x42, 0x7C, 0x44, 0x42, 0x42};
const uint8_t BigFont_C[7] PROGMEM = {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C};
const uint8_t BigFont_L[7] PROGMEM = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E};

typedef struct { int8_t x, y, vx, vy; uint8_t active, isSquashed, squashTimer, natural, dyingTimer, onBox; int8_t platformX; } Goomba;
typedef struct { int8_t x, y; uint8_t active, itemType, isHit, stopTimer; } FallingBox;
typedef struct { int8_t x, y; uint8_t active; } Debris;

int8_t marioX = 26, marioY;
uint8_t marioDir = 0, prevS2 = 0, marioFrame=0, isJumping=0, jumpPhase=0;
uint8_t coinCount=0, lives=2;
uint8_t gameState = 3;

Goomba goombas[MAX_GOOMBAS];
FallingBox boxes[MAX_BOXES];
Debris debris[MAX_DEBRIS];

uint8_t invincibleTimer = 0, spawnTimer = 0;
// 버퍼를 하나로 통합하여 재사용 (메모리 절약)
uint8_t displayBuffer[64];

volatile uint8_t soundMode = 0;
volatile uint32_t soundFrame = 0;
volatile uint8_t soundActive = 0;

// ============================================================================
// --- 사운드 ---
// ============================================================================
ISR(TIMER1_COMPA_vect) { PORTC ^= (1 << BUZZER_PIN); }
void setBuzzerFreq(uint16_t freq) {
	if (freq == 0) { TIMSK1 &= ~(1 << OCIE1A); PORTC &= ~(1 << BUZZER_PIN); soundActive = 0; }
	else { OCR1A = (uint16_t)((F_CPU / (2UL * 8 * freq)) - 1); TCNT1 = 0; TIMSK1 |= (1 << OCIE1A); soundActive = 1; }
}
void triggerJumpSound() { soundMode = 1; soundFrame = 0; }
void triggerCoinSound() { soundMode = 2; soundFrame = 0; }
void triggerSquashSound() { soundMode = 3; soundFrame = 0; }
void triggerGameOverSound() { soundMode = 4; soundFrame = 0; }
void triggerGameClearSound() { soundMode = 5; soundFrame = 0; }
void triggerHurtSound() { soundMode = 6; soundFrame = 0; }

void soundLogic() {
	if (soundMode == 0) return;
	soundFrame++;
	if (soundMode == 1) { // Jump
		if (soundFrame == 1) setBuzzerFreq(131); else if (soundFrame == 3) setBuzzerFreq(262); else if (soundFrame == 6) { setBuzzerFreq(0); soundMode = 0; }
		} else if (soundMode == 2) { // Coin
		if (soundFrame == 1) setBuzzerFreq(880); else if (soundFrame == 3) setBuzzerFreq(988); else if (soundFrame == 6) { setBuzzerFreq(0); soundMode = 0; }
		} else if (soundMode == 3) { // Squash
		if (soundFrame < 5) setBuzzerFreq(150 - (soundFrame * 20)); else { setBuzzerFreq(0); soundMode = 0; }
		} else if (soundMode == 4) { // Game Over
		if (soundFrame < 80) setBuzzerFreq(523 - (soundFrame * 5)); else { setBuzzerFreq(0); soundMode = 0; }
		} else if (soundMode == 5) { // Clear
		if (soundFrame < 20) setBuzzerFreq(262 + (soundFrame * 50)); else { setBuzzerFreq(0); soundMode = 0; }
		} else if (soundMode == 6) { // Hurt
		if (soundFrame < 3) setBuzzerFreq(2000); else if (soundFrame < 6) setBuzzerFreq(0); else if (soundFrame < 9) setBuzzerFreq(2000); else { setBuzzerFreq(0); soundMode = 0; }
	}
}

// ============================================================================
// --- I2C (TM1650) ---
// ============================================================================
#define TM_SCL_HI (PORTC |= (1 << TM_SCL_PIN))
#define TM_SCL_LO (PORTC &= ~(1 << TM_SCL_PIN))
#define TM_SDA_HI (PORTC |= (1 << TM_SDA_PIN))
#define TM_SDA_LO (PORTC &= ~(1 << TM_SDA_PIN))
#define TM_SCL_OUT (DDRC |= (1 << TM_SCL_PIN)) // <-- 이 매크로가 이제 정의됩니다.
#define TM_SDA_OUT (DDRC |= (1 << TM_SDA_PIN))
#define TM_SDA_IN  { DDRC &= ~(1 << TM_SDA_PIN); PORTC |= (1 << TM_SDA_PIN); }
#define TM_SDA_READ (PINC & (1 << TM_SDA_PIN))

void si2c_delay() { _delay_us(2); }

// *** [중요] 누락되었던 함수 추가됨 ***
void si2c_init() {
	TM_SCL_OUT;
	TM_SDA_OUT;
	TM_SCL_HI;
	TM_SDA_HI;
}

void si2c_start() { TM_SDA_OUT; TM_SDA_HI; TM_SCL_HI; si2c_delay(); TM_SDA_LO; si2c_delay(); TM_SCL_LO; }
void si2c_stop() { TM_SDA_OUT; TM_SCL_LO; TM_SDA_LO; si2c_delay(); TM_SCL_HI; si2c_delay(); TM_SDA_HI; si2c_delay(); }
void si2c_write(uint8_t data) { TM_SDA_OUT; for(uint8_t i=0;i<8;i++){TM_SCL_LO;if(data&0x80)TM_SDA_HI;else TM_SDA_LO;si2c_delay();TM_SCL_HI;si2c_delay();data<<=1;} TM_SCL_LO;TM_SDA_IN;TM_SCL_HI;si2c_delay();TM_SCL_LO;TM_SDA_OUT; }
uint8_t si2c_read() { uint8_t data=0;TM_SDA_IN;for(uint8_t i=0;i<8;i++){TM_SCL_LO;si2c_delay();TM_SCL_HI;si2c_delay();data<<=1;if(TM_SDA_READ)data|=1;}TM_SCL_LO;TM_SDA_OUT;TM_SDA_LO;TM_SCL_HI;si2c_delay();TM_SCL_LO;return data;}
uint8_t readTM1650() { si2c_start(); si2c_write(0x4F); uint8_t k=si2c_read(); si2c_stop(); return k; }

// ============================================================================
// --- 그래픽 ---
// ============================================================================
void drawSprite(uint8_t* buffer, int8_t x, int8_t y, uint8_t w, uint8_t h, const uint16_t* r, const uint16_t* g, const uint16_t* b, int8_t targetY, uint8_t isUpperRow, int8_t frameIdx, uint8_t flip) {
	int8_t localY = targetY - y;
	if (localY < 0 || localY >= h) return;
	uint8_t maskR = isUpperRow ? (1 << BIT_R1) : (1 << BIT_R2);
	uint8_t maskG = isUpperRow ? (1 << BIT_G1) : (1 << BIT_G2);
	uint8_t maskB = isUpperRow ? (1 << BIT_B1) : (1 << BIT_B2);
	int16_t idx = (frameIdx >= 0) ? ((int16_t)frameIdx * h + localY) : localY;
	uint16_t rr = pgm_read_word(&r[idx]);
	uint16_t gg = pgm_read_word(&g[idx]);
	uint16_t bb = (b) ? pgm_read_word(&b[idx]) : 0;
	int8_t startX = (x < 0) ? 0 : x;
	int8_t endX = (x + w > 64) ? 64 : x + w;
	for (int8_t col = startX; col < endX; col++) {
		int8_t spriteCol = col - x;
		uint8_t bitPos = flip ? spriteCol : (w - 1 - spriteCol);
		if ((rr >> bitPos) & 1) buffer[col] |= maskR;
		if ((gg >> bitPos) & 1) buffer[col] |= maskG;
		if ((bb >> bitPos) & 1) buffer[col] |= maskB;
	}
}
void drawBitmap(uint8_t* buffer, int8_t x, int8_t y, uint8_t w, uint8_t h, const uint8_t* data, int8_t targetY, uint8_t isUpperRow, uint8_t r, uint8_t g, uint8_t b) {
	int8_t localY = targetY - y;
	if (localY < 0 || localY >= h) return;
	uint8_t mask = 0;
	if (isUpperRow) { if(r) mask |= (1<<BIT_R1); if(g) mask |= (1<<BIT_G1); if(b) mask |= (1<<BIT_B1); }
	else { if(r) mask |= (1<<BIT_R2); if(g) mask |= (1<<BIT_G2); if(b) mask |= (1<<BIT_B2); }
	uint8_t rowData = pgm_read_byte(&data[localY]);
	for (int8_t col = 0; col < w; col++) {
		int8_t screenX = x + col;
		if (screenX >= 0 && screenX < 64) { if ((rowData >> (w - 1 - col)) & 1) buffer[screenX] |= mask; }
	}
}
void drawDebris(uint8_t* buffer, int8_t x, int8_t y, int8_t targetY, uint8_t isUpperRow) {
	int8_t localY = targetY - y; if (localY < 0 || localY >= DEBRIS_SIZE) return;
	uint8_t mask = isUpperRow ? ((1<<BIT_R1)|(1<<BIT_G1)) : ((1<<BIT_R2)|(1<<BIT_G2));
	for(uint8_t i=0; i<DEBRIS_SIZE; i++) { int8_t sx = x + i; if(sx >= 0 && sx < 64) buffer[sx] |= mask; }
}
void drawHudPixel(uint8_t* buffer, int8_t x, int8_t y, int8_t targetY, uint8_t isUpperRow) {
	if (targetY != y) return; if (x < 0 || x >= 64) return;
	uint8_t mask = isUpperRow ? ((1<<BIT_R1)|(1<<BIT_G1)|(1<<BIT_B1)) : ((1<<BIT_R2)|(1<<BIT_G2)|(1<<BIT_B2));
	buffer[x] |= mask;
}

// 버퍼 채우기 함수 (Logical Y 좌표를 받아 해당 라인의 데이터 생성)
void fillBuffer(uint8_t* buffer, int8_t logicalUpperY, int8_t logicalLowerY) {
	memset(buffer, 0, 64);
	if (gameState == 0) {
		if (invincibleTimer == 0 || (invincibleTimer % 4 < 2)) {
			drawSprite(buffer, marioX, marioY, MARIO_W, MARIO_H, MarioR[0], MarioG[0], MarioB[0], logicalUpperY, 1, isJumping ? 1 : marioFrame, marioDir);
			drawSprite(buffer, marioX, marioY, MARIO_W, MARIO_H, MarioR[0], MarioG[0], MarioB[0], logicalLowerY, 0, isJumping ? 1 : marioFrame, marioDir);
		}
		for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active) {
			if(!goombas[i].isSquashed) {
				drawSprite(buffer, goombas[i].x, goombas[i].y, GOOMBA_W, GOOMBA_H, GoombaR, GoombaG, GoombaB, logicalUpperY, 1, -1, 0);
				drawSprite(buffer, goombas[i].x, goombas[i].y, GOOMBA_W, GOOMBA_H, GoombaR, GoombaG, GoombaB, logicalLowerY, 0, -1, 0);
				} else {
				drawSprite(buffer, goombas[i].x, goombas[i].y + (GOOMBA_H - SQUASH_H), GOOMBA_W, SQUASH_H, SquashR, SquashG, NULL, logicalUpperY, 1, -1, 0);
				drawSprite(buffer, goombas[i].x, goombas[i].y + (GOOMBA_H - SQUASH_H), GOOMBA_W, SQUASH_H, SquashR, SquashG, NULL, logicalLowerY, 0, -1, 0);
			}
		}
		for(uint8_t i=0; i<MAX_BOXES; i++) if(boxes[i].active) {
			const uint16_t* br = boxes[i].isHit ? BoxHitR : BoxR; const uint16_t* bg = boxes[i].isHit ? BoxHitG : BoxG; const uint16_t* bb = boxes[i].isHit ? BoxHitB : BoxB;
			drawSprite(buffer, boxes[i].x, boxes[i].y, BOX_W, BOX_H, br, bg, bb, logicalUpperY, 1, -1, 0);
			drawSprite(buffer, boxes[i].x, boxes[i].y, BOX_W, BOX_H, br, bg, bb, logicalLowerY, 0, -1, 0);
			if (boxes[i].isHit) {
				int8_t itemY = boxes[i].y - 10;
				if (boxes[i].itemType == 0) {
					drawBitmap(buffer, boxes[i].x + (BOX_W-COIN_W)/2, itemY, COIN_W, COIN_H, CoinR, logicalUpperY, 1, 1, 1, 0);
					drawBitmap(buffer, boxes[i].x + (BOX_W-COIN_W)/2, itemY, COIN_W, COIN_H, CoinR, logicalLowerY, 0, 1, 1, 0);
					} else if (boxes[i].itemType == 1) {
					drawBitmap(buffer, boxes[i].x + 3, itemY + 2, 5, 5, HeartIcon, logicalUpperY, 1, 1, 0, 0);
					drawBitmap(buffer, boxes[i].x + 3, itemY + 2, 5, 5, HeartIcon, logicalLowerY, 0, 1, 0, 0);
				}
			}
		}
		for(uint8_t i=0; i<MAX_DEBRIS; i++) if(debris[i].active) {
			drawDebris(buffer, debris[i].x, debris[i].y, logicalUpperY, 1);
			drawDebris(buffer, debris[i].x, debris[i].y, logicalLowerY, 0);
		}
		if (logicalUpperY >= GROUND_Y) for (uint8_t i = 0; i < 64; i++) buffer[i] |= (1 << BIT_G1) | (1 << BIT_R1);
		if (logicalLowerY >= GROUND_Y) for (uint8_t i = 0; i < 64; i++) buffer[i] |= (1 << BIT_G2) | (1 << BIT_R2);
		
		drawBitmap(buffer, 2, 2, COIN_W, COIN_H, CoinR, logicalUpperY, 1, 1, 1, 0);
		drawHudPixel(buffer, 8, 4, logicalUpperY, 1); drawHudPixel(buffer, 10, 4, logicalUpperY, 1);
		drawHudPixel(buffer, 9, 5, logicalUpperY, 1); drawHudPixel(buffer, 8, 6, logicalUpperY, 1); drawHudPixel(buffer, 10, 6, logicalUpperY, 1);
		drawBitmap(buffer, 11, 3, 5, 5, NumFont[coinCount%10], logicalUpperY, 1, 1, 1, 1);
		for(uint8_t h=0; h<lives; h++) { int8_t hx = 58 - (h * 6); if(hx > 30) drawBitmap(buffer, hx, 2, 5, 5, HeartIcon, logicalUpperY, 1, 1, 0, 0); }
		} else if (gameState == 1) { // Game Over
		if (logicalLowerY >= 19 && logicalLowerY < 26) {
			drawBitmap(buffer, 16, 19, 7, 7, BigFont_G, logicalLowerY, 0, 1, 0, 0); drawBitmap(buffer, 24, 19, 7, 7, BigFont_A, logicalLowerY, 0, 1, 0, 0);
			drawBitmap(buffer, 32, 19, 7, 7, BigFont_M, logicalLowerY, 0, 1, 0, 0); drawBitmap(buffer, 40, 19, 7, 7, BigFont_E, logicalLowerY, 0, 1, 0, 0);
		}
		if (logicalUpperY >= 40 && logicalUpperY < 47) {
			drawBitmap(buffer, 16, 40, 7, 7, BigFont_O, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 24, 40, 7, 7, BigFont_V, logicalUpperY, 1, 1, 0, 0);
			drawBitmap(buffer, 32, 40, 7, 7, BigFont_E, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 40, 40, 7, 7, BigFont_R, logicalUpperY, 1, 1, 0, 0);
		}
		} else if (gameState == 2) { // Clear
		if (logicalLowerY >= 19 && logicalLowerY < 26) {
			drawBitmap(buffer, 16, 19, 7, 7, BigFont_G, logicalLowerY, 0, 0, 1, 0); drawBitmap(buffer, 24, 19, 7, 7, BigFont_A, logicalLowerY, 0, 0, 1, 0);
			drawBitmap(buffer, 32, 19, 7, 7, BigFont_M, logicalLowerY, 0, 0, 1, 0); drawBitmap(buffer, 40, 19, 7, 7, BigFont_E, logicalLowerY, 0, 0, 1, 0);
		}
		if (logicalUpperY >= 40 && logicalUpperY < 47) {
			drawBitmap(buffer, 12, 40, 7, 7, BigFont_C, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 20, 40, 7, 7, BigFont_L, logicalUpperY, 1, 0, 1, 0);
			drawBitmap(buffer, 28, 40, 7, 7, BigFont_E, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 36, 40, 7, 7, BigFont_A, logicalUpperY, 1, 0, 1, 0);
			drawBitmap(buffer, 44, 40, 7, 7, BigFont_R, logicalUpperY, 1, 0, 1, 0);
		}
		} else if (gameState == 3) { // Title
		if (logicalLowerY >= 19 && logicalLowerY < 26) {
			drawBitmap(buffer, 16, 19, 7, 7, BigFont_P, logicalLowerY, 0, 1, 1, 1); drawBitmap(buffer, 24, 19, 7, 7, BigFont_U, logicalLowerY, 0, 1, 1, 1);
			drawBitmap(buffer, 32, 19, 7, 7, BigFont_S, logicalLowerY, 0, 1, 1, 1); drawBitmap(buffer, 40, 19, 7, 7, BigFont_H, logicalLowerY, 0, 1, 1, 1);
		}
		if (logicalUpperY >= 40 && logicalUpperY < 47) {
			drawBitmap(buffer, 26, 40, 7, 7, BigFont_S, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 34, 41, 5, 5, NumFont[1], logicalUpperY, 1, 1, 0, 0);
		}
	}
}

// *** 핵심 수정: 스캔 함수 (128클럭 전송) ***
void scanFrame() {
	for (uint8_t row = 0; row < 16; row++) {
		CTRL_PORT |= (1 << OE_PIN); // 화면 끄기
		ROW_PORT = (ROW_PORT & 0xF0) | (row & 0x0F); // 행 선택

		// --- Step 1: 위쪽 스크린 데이터 전송 (Far End) ---
		// 위쪽 스크린은 Y좌표 32~63을 담당합니다.
		// Upper bit: row + 32, Lower bit: row + 32 + 16 (즉 row+48)
		fillBuffer(displayBuffer, row + 32, row + 48);
		for (uint8_t i = 0; i < 64; i++) {
			PORTD = displayBuffer[i];
			CTRL_PORT |= (1 << CLK_PIN); CTRL_PORT &= ~(1 << CLK_PIN);
		}

		// --- Step 2: 아래쪽 스크린 데이터 전송 (Near End) ---
		// 아래쪽 스크린은 Y좌표 0~31을 담당합니다.
		// Upper bit: row, Lower bit: row + 16
		fillBuffer(displayBuffer, row, row + 16);
		for (uint8_t i = 0; i < 64; i++) {
			PORTD = displayBuffer[i];
			CTRL_PORT |= (1 << CLK_PIN); CTRL_PORT &= ~(1 << CLK_PIN);
		}

		// --- Latch & Display ---
		CTRL_PORT |= (1 << LAT_PIN); CTRL_PORT &= ~(1 << LAT_PIN);
		CTRL_PORT &= ~(1 << OE_PIN); // 화면 켜기
		_delay_us(BCM_BASE_DELAY);
		CTRL_PORT |= (1 << OE_PIN); // 화면 끄기
	}
}

// ============================================================================
// --- 게임 로직 ---
// ============================================================================
void spawnNaturalGoomba() {
	uint8_t count = 0; for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active && goombas[i].natural) count++;
	if(count >= 2) return;
	for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(!goombas[i].active) {
		goombas[i].active = 1; goombas[i].isSquashed = 0; goombas[i].natural = 1; goombas[i].dyingTimer = 0; goombas[i].onBox = 0;
		goombas[i].y = GROUND_Y - GOOMBA_H; goombas[i].vy = 0;
		if(rand() % 2 == 0) { goombas[i].x = -GOOMBA_W; goombas[i].vx = 1; } else { goombas[i].x = 64; goombas[i].vx = -1; }
		break;
	}
}
void spawnBoxGoomba(int8_t x, int8_t y, uint8_t isDestroyed) {
	for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(!goombas[i].active) {
		goombas[i].active = 1; goombas[i].natural = 0; goombas[i].isSquashed = 0; goombas[i].dyingTimer = 0;
		goombas[i].vx = (rand()%2) ? 1 : -1; goombas[i].vy = 0;
		if (isDestroyed) { goombas[i].onBox = 0; goombas[i].x = x; goombas[i].y = y - 4; }
		else { goombas[i].onBox = 1; goombas[i].platformX = x; goombas[i].x = x; goombas[i].y = y - GOOMBA_H; }
		break;
	}
}
void createDebris(int8_t x, int8_t y) { for(uint8_t i=0; i<MAX_DEBRIS; i++) if(!debris[i].active) { debris[i].active = 1; debris[i].x = x + (rand() % BOX_W); debris[i].y = y; break; } }
void spawnFallingBox() { for(uint8_t i=0; i<MAX_BOXES; i++) if(!boxes[i].active) { boxes[i].active = 1; boxes[i].isHit = 0; boxes[i].stopTimer = 0; boxes[i].x = rand()%(64-BOX_W); boxes[i].y = -BOX_H; uint8_t r = rand() % 10; boxes[i].itemType = (r<4)?0:(r<7)?1:2; break; } }
void applyItemEffect(uint8_t type) { if(type==0){triggerCoinSound();coinCount++;if(coinCount>=10){gameState=2;triggerGameClearSound();}}else if(type==1){if(lives<5)lives++;else{triggerCoinSound();coinCount++;if(coinCount>=10){gameState=2;triggerGameClearSound();}}} }
void initGame() { marioX = 26; marioY = GROUND_Y - MARIO_H; marioDir = 0; lives = 2; coinCount = 0; invincibleTimer = 0; spawnTimer = 0; memset(goombas, 0, sizeof(goombas)); memset(boxes, 0, sizeof(boxes)); memset(debris, 0, sizeof(debris)); }
void hurtEffect() {
	if(invincibleTimer>0) return; triggerHurtSound(); lives--; invincibleTimer = 60;
	if(lives==0) { gameState=1; triggerGameOverSound(); for(uint8_t k=0; k<120; k++){ for(int s=0; s<2; s++) scanFrame(); soundLogic(); _delay_ms(10); } gameState=3; initGame(); }
}

void physicsStep() {
	soundLogic(); uint8_t key = readTM1650();
	if (gameState == 3) { if (key == 0x44) { gameState = 0; triggerCoinSound(); initGame(); } return; }
	if(invincibleTimer > 0) invincibleTimer--;
	spawnTimer++; if (spawnTimer >= 200) { spawnTimer = 0; if (rand() % 100 < 70) spawnNaturalGoomba(); if (rand() % 100 < 50) spawnFallingBox(); }
	uint8_t s_left=(key==0x45), s_right=(key==0x55), s_jump=(key==0x4C), s_reset=(key==0x5F);
	if(s_reset) { gameState = 3; initGame(); return; }
	if(s_left) { if(marioX>0) marioX--; marioDir=1; marioFrame^=1; } else if(s_right) { if(marioX<52) marioX++; marioDir=0; marioFrame^=1; } else if(!isJumping) marioFrame = 0;
	if(s_jump && !prevS2 && !isJumping) { isJumping=1; jumpPhase=0; triggerJumpSound(); } prevS2 = s_jump;
	if(isJumping) {
		int8_t ny = (GROUND_Y - MARIO_H) + jumpArc[jumpPhase]; marioY = ny;
		if(jumpPhase > 8) {
			for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active && !goombas[i].isSquashed && goombas[i].dyingTimer==0 && ABS((marioX+6)-(goombas[i].x+6))<10 && ABS((marioY+MARIO_H)-goombas[i].y)<=2) {
				goombas[i].isSquashed=1; goombas[i].squashTimer=10; triggerSquashSound(); triggerCoinSound(); coinCount++; if(coinCount>=10) { gameState=2; triggerGameClearSound(); }
			}
		}
		for(uint8_t i=0; i<MAX_BOXES; i++) if(boxes[i].active && !boxes[i].isHit) { if(ABS((marioX+6)-(boxes[i].x+6))<10 && ABS(marioY-(boxes[i].y+BOX_H))<6) { boxes[i].isHit=1; boxes[i].stopTimer=12; if(boxes[i].itemType==2) spawnBoxGoomba(boxes[i].x, boxes[i].y, 0); else applyItemEffect(boxes[i].itemType); } }
		jumpPhase++; if(jumpPhase>=JUMP_LEN) { isJumping=0; marioY=GROUND_Y-MARIO_H; }
	} else marioY = GROUND_Y - MARIO_H;
	for(uint8_t i=0; i<MAX_BOXES; i++) if(boxes[i].active) { if(boxes[i].isHit) { if(boxes[i].stopTimer>0) boxes[i].stopTimer--; else boxes[i].active=0; } else { boxes[i].y++; if(boxes[i].y>=SCREEN_SPLIT_Y) { if(boxes[i].itemType==2) spawnBoxGoomba(boxes[i].x, boxes[i].y, 1); boxes[i].active=0; createDebris(boxes[i].x, boxes[i].y); } } }
	for(uint8_t i=0; i<MAX_DEBRIS; i++) if(debris[i].active) { debris[i].y+=2; if(invincibleTimer==0 && ABS((marioX+6)-(debris[i].x+2))<8 && ABS((marioY+8)-debris[i].y)<8) { debris[i].active=0; hurtEffect(); } if(debris[i].y>64) debris[i].active=0; }
	for(uint8_t i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active) {
		if(goombas[i].dyingTimer>0) { goombas[i].dyingTimer--; if(goombas[i].dyingTimer==0) goombas[i].active=0; } else if(goombas[i].isSquashed) { if(goombas[i].squashTimer>0) goombas[i].squashTimer--; else goombas[i].active=0; }
		else {
			if(goombas[i].onBox) { goombas[i].x+=goombas[i].vx; if(goombas[i].x < goombas[i].platformX || goombas[i].x > goombas[i].platformX+BOX_W) goombas[i].onBox=0; }
			else { if(goombas[i].y < GROUND_Y-GOOMBA_H) goombas[i].y+=2; else { goombas[i].y=GROUND_Y-GOOMBA_H; goombas[i].x+=goombas[i].vx; if(goombas[i].vx>0 && goombas[i].x>64) goombas[i].active=0; if(goombas[i].vx<0 && goombas[i].x<-12) goombas[i].active=0; } }
			if(invincibleTimer==0 && !goombas[i].isSquashed && ABS((marioX+6)-(goombas[i].x+6))<6 && ABS((marioY+8)-(goombas[i].y+6))<8) { hurtEffect(); goombas[i].dyingTimer=12; }
		}
	}
}

int main(void) {
	DDRD |= 0xFC; DDRB |= 0x0F; DDRC |= 0x0F;
	TCCR1A = 0; TCCR1B = (1 << WGM12) | (1 << CS11); OCR1A = 1000; TIMSK1 = 0; sei();
	
	// 수정됨: si2c_init 함수가 정의되었으므로 이제 정상적으로 호출됩니다.
	si2c_init();
	
	_delay_ms(100); si2c_start(); si2c_write(0x48); si2c_write(0x01); si2c_stop();
	srand(123); initGame(); gameState = 3;
	uint8_t loopCount = 0;
	while (1) {
		for(uint8_t k=0; k<2; k++) scanFrame();
		loopCount++; if(loopCount % 3 == 0) physicsStep();
	}
}