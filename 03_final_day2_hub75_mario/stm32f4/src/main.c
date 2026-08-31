/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Super Mario STM32 (Start Screen & Hurt Sound Added)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// --- 하드웨어 매크로 ---
#define HUB_CLK_HI    (GPIOA->BSRR = (1<<6))
#define HUB_CLK_LO    (GPIOA->BSRR = (1<<(6+16)))
#define HUB_LAT_HI    (GPIOA->BSRR = (1<<7))
#define HUB_LAT_LO    (GPIOA->BSRR = (1<<(7+16)))
#define HUB_OE_HI     (GPIOB->BSRR = (1<<0))
#define HUB_OE_LO     (GPIOB->BSRR = (1<<(0+16)))

// TM1650 I2C Pins (PB6, PB7)
#define SCL_PIN GPIO_PIN_6
#define SDA_PIN GPIO_PIN_7
#define I2C_PORT GPIOB

// 부저 핀 (PB8)
#define BUZZER_PIN  GPIO_PIN_8
#define BUZZER_PORT GPIOB

// 데이터 비트 매핑
#define BIT_R1 0
#define BIT_G1 1
#define BIT_B1 2
#define BIT_R2 3
#define BIT_G2 4
#define BIT_B2 5

// 게임 상수
#define SCREEN_W 64
#define SCREEN_H 64
#define GROUND_Y 54
#define TOP_SCREEN_BOTTOM 31
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
#define MAX_BOXES   3
#define MAX_DEBRIS  8

// BCM 설정
#define BCM_PLANES      3
#define BCM_BASE_DELAY  3

#define BCM_LEVEL_MAX   ((1 << BCM_PLANES) - 1)

// 굼바 색상
#define GOOMBA_BODY_R_LVL  BCM_LEVEL_MAX
#define GOOMBA_BODY_G_LVL  (BCM_LEVEL_MAX / 2)
#define GOOMBA_BODY_B_LVL  0

#define GOOMBA_EYE_R_LVL   BCM_LEVEL_MAX
#define GOOMBA_EYE_G_LVL   BCM_LEVEL_MAX
#define GOOMBA_EYE_B_LVL   BCM_LEVEL_MAX

static uint8_t g_curBcmPlane = 0;

// 사운드 상태 관리 변수
// 0:None, 1:Jump, 2:Coin, 3:Squash, 4:GameOver, 5:GameClear, 6:Hurt(BeepBeep)
volatile uint8_t soundMode = 0;
volatile uint32_t soundFrame = 0;
volatile uint8_t soundActive = 0;

// 구조체
typedef struct {
    int x, y;
    int8_t vx, vy;
    uint8_t active, isSquashed, squashTimer, natural, dyingTimer, onBox;
    int platformX;
} Goomba;

typedef struct { int x, y; uint8_t active, itemType, isHit, stopTimer; } FallingBox;
typedef struct { int x, y; uint8_t active; } Debris;

// 전역 변수
Goomba    goombas[MAX_GOOMBAS];
FallingBox boxes[MAX_BOXES];
Debris    debris[MAX_DEBRIS];

int marioX = 26, marioY;
uint8_t marioDir = 0, marioFrame = 0, isJumping = 0, jumpPhase = 0;
uint8_t prevS2 = 0;
// gameState: 0=Playing, 1=GameOver, 2=Clear, 3=Title(Start Screen)
int     coinCount = 0, lives = 2, gameState = 3;
uint16_t invincibleTimer = 0, spawnTimer = 0;

// 라인 버퍼
uint8_t topBuffer[64];
uint8_t bottomBuffer[64];

const int8_t jumpArc[] = {
    -4, -8, -12, -15, -18, -20, -21, -22,
    -22, -21, -20, -18, -15, -12, -8, -4
};

// 스프라이트 데이터
const uint16_t MarioR[2][16] = {{0x0F8,0x1FC,0x1C6,0x1DF,0x1DF,0x1C6,0x0F8,0x1FC,0x3FE,0x3FE,0x3FE,0x30E,0x000,0x0F8,0x1FC,0x000},{0x0F8,0x1FC,0x1C6,0x1DF,0x1DF,0x1C6,0x0F8,0x1FC,0x3FE,0x3FE,0x32E,0x30E,0x18C,0x39C,0x73E,0x600}};
const uint16_t MarioG[2][16] = {{0x000,0x000,0x1C6,0x1DF,0x1DF,0x1C6,0x000,0x000,0x0D8,0x0D8,0x000,0x000,0x000,0x0F8,0x1FC,0x000},{0x000,0x000,0x1C6,0x1DF,0x1DF,0x1C6,0x000,0x000,0x0D8,0x0D8,0x020,0x000,0x000,0x39C,0x73E,0x600}};
const uint16_t MarioB[2][16] = {{0x000,0x000,0x006,0x01F,0x01F,0x006,0x000,0x020,0x0D8,0x0FC,0x0FC,0x078,0x0FC,0x000,0x000,0x000},{0x000,0x000,0x006,0x01F,0x01F,0x006,0x000,0x020,0x0D8,0x0FC,0x0DC,0x078,0x1FC,0x000,0x000,0x000}};

const uint16_t GoombaR[12] = {0x03C0, 0x07E0, 0x0FF0, 0x1FF8,0x399C, 0x3FFC, 0x1FF8, 0x0FF0,0x05A0, 0x0DB0, 0x0C30, 0x0000};
const uint16_t GoombaG[12] = {0x0000, 0x0000, 0x0000, 0x0000,0x0000, 0x0660, 0x0660, 0x03C0,0x0240, 0x0000, 0x0000, 0x0000};
const uint16_t GoombaB[12] = {0x0000, 0x0000, 0x0000, 0x0000,0x0000, 0x0660, 0x0660, 0x03C0,0x0000, 0x0000, 0x0000, 0x0000};

const uint16_t SquashR[6] = { 0x07E0, 0x1FF8, 0x399C, 0x3FFC, 0x1FF8, 0x0000 };
const uint16_t SquashG[6] = { 0x0000, 0x0000, 0x0660, 0x0660, 0x0000, 0x0000 };

const uint16_t BoxR[12] = {0x0FFF, 0x0FFF, 0x0801, 0x0BE1,0x0BF1, 0x0831, 0x0861, 0x08C1,0x0801, 0x08C1, 0x0FFF, 0x0FFF};
const uint16_t BoxG[12] = {0x0FFF, 0x0FFF, 0x0801, 0x0BE1,0x0BF1, 0x0831, 0x0861, 0x08C1,0x0801, 0x08C1, 0x0FFF, 0x0FFF};
const uint16_t BoxB[12] = {0x0000, 0x0000, 0x0000, 0x03E0,0x03F0, 0x0030, 0x0060, 0x00C0,0x0000, 0x00C0, 0x0000, 0x0000};

const uint16_t BoxHitR[12] = {0x0FFF,0x0801,0x0801,0x0BE1,0x0BF1,0x0831,0x0861,0x08C1,0x0801,0x08C1,0x0801,0x0FFF};
const uint16_t BoxHitG[12] = {0x0888,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0888};
const uint16_t BoxHitB[12] = {0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};

const uint8_t CoinR[7] = { 0x0E, 0x11, 0x15, 0x15, 0x15, 0x11, 0x0E };
const uint8_t HeartIcon[5] = { 0b01010, 0b11111, 0b11111, 0b01110, 0b00100 };
const uint8_t NumFont[11][5] = {{0x02, 0x05, 0x05, 0x05, 0x02},{0x02, 0x02, 0x02, 0x02, 0x02},{0x07, 0x01, 0x07, 0x04, 0x07},{0x07, 0x01, 0x03, 0x01, 0x07},{0x05, 0x05, 0x07, 0x01, 0x01},{0x07, 0x04, 0x07, 0x01, 0x07},{0x07, 0x04, 0x07, 0x05, 0x07},{0x07, 0x01, 0x01, 0x02, 0x02},{0x07, 0x05, 0x02, 0x05, 0x07},{0x07, 0x05, 0x07, 0x01, 0x07},{0x00, 0x02, 0x01, 0x02, 0x00}};

// 폰트 데이터 (BigFont)
const uint8_t BigFont_G[7] = {0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C};
const uint8_t BigFont_A[7] = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42};
const uint8_t BigFont_M[7] = {0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x42};
const uint8_t BigFont_E[7] = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E};
const uint8_t BigFont_O[7] = {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C};
const uint8_t BigFont_V[7] = {0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x08};
const uint8_t BigFont_R[7] = {0x7C, 0x42, 0x42, 0x7C, 0x44, 0x42, 0x42};
const uint8_t BigFont_C[7] = {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C};
const uint8_t BigFont_L[7] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E};
// [추가] Start Screen용 폰트 (P, U, S, H)
const uint8_t BigFont_P[7] = {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40};
const uint8_t BigFont_U[7] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C};
const uint8_t BigFont_S[7] = {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x42, 0x3C};
const uint8_t BigFont_H[7] = {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM4_Init(void); // 타이머 초기화 함수
void soundLogic(void); // 사운드 로직 선언
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void delay_us(uint32_t us) {
    uint32_t count = us * (SystemCoreClock / 1000000 / 4);
    while(count--) __asm("nop");
}

// --- Direct Register Access Sound System ---

// 주파수 설정 (TIM4 레지스터 직접 제어)
void setBuzzerFreq(uint32_t freq) {
    if (freq == 0) {
        TIM4->CR1 &= ~TIM_CR1_CEN; // Timer Stop
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        soundActive = 0;
    } else {
        uint32_t arr = (1000000 / (2 * freq));
        if (arr > 0) arr--;

        TIM4->ARR = arr;
        TIM4->CNT = 0;

        if (!soundActive) {
            TIM4->CR1 |= TIM_CR1_CEN; // Timer Start
            soundActive = 1;
        }
    }
}

// 사운드 트리거 함수들
void triggerJumpSound() { soundMode = 1; soundFrame = 0; }
void triggerCoinSound() { soundMode = 2; soundFrame = 0; }
void triggerSquashSound() { soundMode = 3; soundFrame = 0; }
void triggerGameOverSound() { soundMode = 4; soundFrame = 0; }
void triggerGameClearSound() { soundMode = 5; soundFrame = 0; }
void triggerHurtSound() { soundMode = 6; soundFrame = 0; } // [추가] 삐빅 사운드

// 사운드 로직 (매 프레임 33ms 호출)
void soundLogic() {
    if (soundMode == 0) return;
    soundFrame++;

    // 1. 점프
    if (soundMode == 1) {
        if (soundFrame == 1) setBuzzerFreq(131);
        else if (soundFrame == 3) setBuzzerFreq(262);
        else if (soundFrame == 6) { setBuzzerFreq(0); soundMode = 0; }
    }
    // 2. 코인
    else if (soundMode == 2) {
        if (soundFrame == 1) setBuzzerFreq(880);
        else if (soundFrame == 3) setBuzzerFreq(988);
        else if (soundFrame == 6) { setBuzzerFreq(0); soundMode = 0; }
    }
    // 3. 밟기
    else if (soundMode == 3) {
        if (soundFrame < 5) setBuzzerFreq(150 - (soundFrame * 20));
        else { setBuzzerFreq(0); soundMode = 0; }
    }
    // 4. Game Over
    else if (soundMode == 4) {
        if (soundFrame == 1) setBuzzerFreq(523); // C5
        else if (soundFrame == 6) setBuzzerFreq(0);
        else if (soundFrame == 8) setBuzzerFreq(392); // G4
        else if (soundFrame == 13) setBuzzerFreq(0);
        else if (soundFrame == 15) setBuzzerFreq(330); // E4
        else if (soundFrame == 20) setBuzzerFreq(0);
        else if (soundFrame == 25) setBuzzerFreq(440); // A4
        else if (soundFrame == 31) setBuzzerFreq(0);
        else if (soundFrame == 33) setBuzzerFreq(494); // B4
        else if (soundFrame == 39) setBuzzerFreq(0);
        else if (soundFrame == 41) setBuzzerFreq(440); // A4
        else if (soundFrame == 47) setBuzzerFreq(0);
        else if (soundFrame == 50) setBuzzerFreq(415); // Ab4
        else if (soundFrame == 57) setBuzzerFreq(0);
        else if (soundFrame == 59) setBuzzerFreq(466); // Bb4
        else if (soundFrame == 66) setBuzzerFreq(0);
        else if (soundFrame == 68) setBuzzerFreq(415); // Ab4
        else if (soundFrame == 76) setBuzzerFreq(0);
        else if (soundFrame == 79) setBuzzerFreq(392); // G4
        else if (soundFrame == 120) { setBuzzerFreq(0); soundMode = 0; }
    }
    // 5. Game Clear
    else if (soundMode == 5) {
        if (soundFrame == 1) setBuzzerFreq(262); // C4
        else if (soundFrame == 4) setBuzzerFreq(330); // E4
        else if (soundFrame == 7) setBuzzerFreq(392); // G4
        else if (soundFrame == 10) setBuzzerFreq(523); // C5
        else if (soundFrame == 13) setBuzzerFreq(659); // E5
        else if (soundFrame == 16) setBuzzerFreq(784); // G5
        else if (soundFrame == 19) setBuzzerFreq(1047); // C6
        else if (soundFrame == 50) { setBuzzerFreq(0); soundMode = 0; }
    }
    // 6. [추가] Hurt (Ppi-ppik!)
    else if (soundMode == 6) {
        if (soundFrame >= 1 && soundFrame <= 3) setBuzzerFreq(2000); // 삐
        else if (soundFrame >= 4 && soundFrame <= 6) setBuzzerFreq(0); // (잠깐 멈춤)
        else if (soundFrame >= 7 && soundFrame <= 9) setBuzzerFreq(2000); // 빅!
        else if (soundFrame >= 10) { setBuzzerFreq(0); soundMode = 0; }
    }
}

// I2C Functions
void i2c_clk(uint8_t lvl) { HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, lvl ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void i2c_dat(uint8_t lvl) { HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, lvl ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void i2c_sda_out() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SDA_PIN; GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}
void i2c_sda_in() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SDA_PIN; GPIO_InitStruct.Mode = GPIO_MODE_INPUT; GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}
void i2c_start() { i2c_sda_out(); i2c_dat(1); i2c_clk(1); delay_us(5); i2c_dat(0); delay_us(5); i2c_clk(0); }
void i2c_stop() { i2c_sda_out(); i2c_clk(0); i2c_dat(0); delay_us(5); i2c_clk(1); delay_us(5); i2c_dat(1); }
void i2c_write(uint8_t data) {
    i2c_sda_out();
    for(int i=0; i<8; i++) { i2c_clk(0); i2c_dat(data & 0x80); delay_us(2); i2c_clk(1); delay_us(2); data <<= 1; }
    i2c_clk(0); i2c_sda_in(); i2c_clk(1); delay_us(5); i2c_clk(0);
}
uint8_t i2c_read() {
    i2c_sda_in(); uint8_t data=0;
    for(int i=0; i<8; i++) { i2c_clk(0); delay_us(2); i2c_clk(1); delay_us(2); data <<= 1; if(HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)) data |= 1; }
    i2c_clk(0); i2c_sda_out(); i2c_dat(0); i2c_clk(1); delay_us(5); i2c_clk(0); return data;
}
uint8_t readTM1650() { i2c_start(); i2c_write(0x4F); uint8_t k = i2c_read(); i2c_stop(); return k; }

void drawSprite(uint8_t* buffer, int x, int y, int w, int h, const uint16_t* r, const uint16_t* g, const uint16_t* b, int targetY, uint8_t isUpperRow, int frameIdx, uint8_t flip) {
    int localY = targetY - y; if (localY < 0 || localY >= h) return;
    uint8_t maskR = isUpperRow ? (1 << BIT_R1) : (1 << BIT_R2);
    uint8_t maskG = isUpperRow ? (1 << BIT_G1) : (1 << BIT_G2);
    uint8_t maskB = isUpperRow ? (1 << BIT_B1) : (1 << BIT_B2);
    int idx = (frameIdx >= 0) ? (frameIdx * h + localY) : localY;
    uint16_t rr = r[idx]; uint16_t gg = g[idx]; uint16_t bb = (b) ? b[idx] : 0;
    int startX = (x < 0) ? 0 : x; int endX = (x + w > 64) ? 64 : x + w;
    for (int col = startX; col < endX; col++) {
        int spriteCol = col - x; uint8_t bitPos = flip ? spriteCol : (w - 1 - spriteCol);
        if ((rr >> bitPos) & 1) buffer[col] |= maskR;
        if ((gg >> bitPos) & 1) buffer[col] |= maskG;
        if ((bb >> bitPos) & 1) buffer[col] |= maskB;
    }
}

void drawBitmap(uint8_t* buffer, int x, int y, int w, int h, const uint8_t* data, int targetY, uint8_t isUpperRow, uint8_t r, uint8_t g, uint8_t b) {
    int localY = targetY - y; if (localY < 0 || localY >= h) return;
    uint8_t mask = 0;
    if (isUpperRow) { if(r) mask |= (1<<BIT_R1); if(g) mask |= (1<<BIT_G1); if(b) mask |= (1<<BIT_B1); }
    else { if(r) mask |= (1<<BIT_R2); if(g) mask |= (1<<BIT_G2); if(b) mask |= (1<<BIT_B2); }
    uint8_t rowData = data[localY];
    for (int col = 0; col < w; col++) {
        int screenX = x + col;
        if (screenX >= 0 && screenX < 64) { if ((rowData >> (w - 1 - col)) & 1) buffer[screenX] |= mask; }
    }
}

void drawDebris(uint8_t* buffer, int x, int y, int targetY, uint8_t isUpperRow) {
    int localY = targetY - y; if (localY < 0 || localY >= DEBRIS_SIZE) return;
    uint8_t mask = isUpperRow ? ((1<<BIT_R1)|(1<<BIT_G1)) : ((1<<BIT_R2)|(1<<BIT_G2));
    for(int i=0; i<DEBRIS_SIZE; i++) { int sx = x + i; if(sx >= 0 && sx < 64) buffer[sx] |= mask; }
}

void drawHudPixel(uint8_t* buffer, int x, int y, int targetY, uint8_t isUpperRow) {
    if (targetY != y) return;
    if (x < 0 || x >= 64) return;
    uint8_t mask = isUpperRow ? ((1<<BIT_R1)|(1<<BIT_G1)|(1<<BIT_B1)) : ((1<<BIT_R2)|(1<<BIT_G2)|(1<<BIT_B2));
    buffer[x] |= mask;
}

static void drawGoombaBCMRow(uint8_t* buffer, int x, int y, int w, int h, const uint16_t* rArr, const uint16_t* gArr, const uint16_t* bArr, int targetY, uint8_t isUpperRow) {
    int localY = targetY - y; if (localY < 0 || localY >= h) return;
    uint16_t rr = rArr[localY]; uint16_t gg = (gArr ? gArr[localY] : 0); uint16_t bb = (bArr ? bArr[localY] : 0);
    int startX = (x < 0) ? 0 : x; int endX = (x + w > 64) ? 64 : x + w;
    uint8_t planeBit = (1U << g_curBcmPlane);
    for (int col = startX; col < endX; col++) {
        int spriteCol = col - x; uint8_t bitPos = (w - 1 - spriteCol); uint16_t mask = (1U << bitPos);
        if ((rr & mask) == 0 && (gg & mask) == 0 && (bb & mask) == 0) continue;
        uint8_t rLevel = 0, gLevel = 0, bLevel = 0;
        if (rr & mask) { rLevel = GOOMBA_BODY_R_LVL; gLevel = GOOMBA_BODY_G_LVL; bLevel = GOOMBA_BODY_B_LVL; }
        if ((gg & mask) || (bb & mask)) { rLevel = GOOMBA_EYE_R_LVL; gLevel = GOOMBA_EYE_G_LVL; bLevel = GOOMBA_EYE_B_LVL; }
        uint8_t rOn = (rLevel & planeBit) ? 1 : 0; uint8_t gOn = (gLevel & planeBit) ? 1 : 0; uint8_t bOn = (bLevel & planeBit) ? 1 : 0;
        if (!rOn && !gOn && !bOn) continue;
        if (isUpperRow) { if (rOn) buffer[col] |= (1 << BIT_R1); if (gOn) buffer[col] |= (1 << BIT_G1); if (bOn) buffer[col] |= (1 << BIT_B1); }
        else { if (rOn) buffer[col] |= (1 << BIT_R2); if (gOn) buffer[col] |= (1 << BIT_G2); if (bOn) buffer[col] |= (1 << BIT_B2); }
    }
}

void fillBuffer(uint8_t* buffer, int logicalUpperY, int logicalLowerY) {
    for (int i = 0; i < 64; i++) buffer[i] = 0;
    if (gameState == 0) {
        if (invincibleTimer == 0 || (invincibleTimer % 4 < 2)) {
            drawSprite(buffer, marioX, marioY, MARIO_W, MARIO_H, MarioR[0], MarioG[0], MarioB[0], logicalUpperY, 1, isJumping ? 1 : marioFrame, marioDir);
            drawSprite(buffer, marioX, marioY, MARIO_W, MARIO_H, MarioR[0], MarioG[0], MarioB[0], logicalLowerY, 0, isJumping ? 1 : marioFrame, marioDir);
        }
        for(int i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active) {
            if(!goombas[i].isSquashed) {
                drawGoombaBCMRow(buffer, goombas[i].x, goombas[i].y, GOOMBA_W, GOOMBA_H, GoombaR, GoombaG, GoombaB, logicalUpperY, 1);
                drawGoombaBCMRow(buffer, goombas[i].x, goombas[i].y, GOOMBA_W, GOOMBA_H, GoombaR, GoombaG, GoombaB, logicalLowerY, 0);
            } else {
                int sy = goombas[i].y + (GOOMBA_H - SQUASH_H);
                drawGoombaBCMRow(buffer, goombas[i].x, sy, GOOMBA_W, SQUASH_H, SquashR, SquashG, NULL, logicalUpperY, 1);
                drawGoombaBCMRow(buffer, goombas[i].x, sy, GOOMBA_W, SQUASH_H, SquashR, SquashG, NULL, logicalLowerY, 0);
            }
        }
        for(int i=0; i<MAX_BOXES; i++) if(boxes[i].active) {
            const uint16_t* br = boxes[i].isHit ? BoxHitR : BoxR; const uint16_t* bg = boxes[i].isHit ? BoxHitG : BoxG; const uint16_t* bb = boxes[i].isHit ? BoxHitB : BoxB;
            drawSprite(buffer, boxes[i].x, boxes[i].y, BOX_W, BOX_H, br, bg, bb, logicalUpperY, 1, -1, 0);
            drawSprite(buffer, boxes[i].x, boxes[i].y, BOX_W, BOX_H, br, bg, bb, logicalLowerY, 0, -1, 0);
            if (boxes[i].isHit) {
                int itemY = boxes[i].y - 10;
                if (boxes[i].itemType == 0) {
                    drawBitmap(buffer, boxes[i].x + (BOX_W-COIN_W)/2, itemY, COIN_W, COIN_H, CoinR, logicalUpperY, 1, 1, 1, 0);
                    drawBitmap(buffer, boxes[i].x + (BOX_W-COIN_W)/2, itemY, COIN_W, COIN_H, CoinR, logicalLowerY, 0, 1, 1, 0);
                } else if (boxes[i].itemType == 1) {
                     drawBitmap(buffer, boxes[i].x + 3, itemY + 2, 5, 5, HeartIcon, logicalUpperY, 1, 1, 0, 0);
                     drawBitmap(buffer, boxes[i].x + 3, itemY + 2, 5, 5, HeartIcon, logicalLowerY, 0, 1, 0, 0);
                }
            }
        }
        for(int i=0; i<MAX_DEBRIS; i++) if(debris[i].active) { drawDebris(buffer, debris[i].x, debris[i].y, logicalUpperY, 1); drawDebris(buffer, debris[i].x, debris[i].y, logicalLowerY, 0); }
        if (logicalUpperY >= GROUND_Y) for (int i = 0; i < 64; i++) buffer[i] |= (1 << BIT_G1) | (1 << BIT_R1);
        if (logicalLowerY >= GROUND_Y) for (int i = 0; i < 64; i++) buffer[i] |= (1 << BIT_G2) | (1 << BIT_R2);
        drawBitmap(buffer, 2, 2, COIN_W, COIN_H, CoinR, logicalUpperY, 1, 1, 1, 0);
        drawHudPixel(buffer, 8, 4, logicalUpperY, 1); drawHudPixel(buffer,10, 4, logicalUpperY, 1); drawHudPixel(buffer, 9, 5, logicalUpperY, 1); drawHudPixel(buffer, 8, 6, logicalUpperY, 1); drawHudPixel(buffer,10, 6, logicalUpperY, 1);
        drawBitmap(buffer, 11, 3, 5, 5, NumFont[coinCount%10], logicalUpperY, 1, 1, 1, 1);
        for(int h=0; h<lives; h++) { int hx = 58 - (h * 6); if(hx > 30) drawBitmap(buffer, hx, 2, 5, 5, HeartIcon, logicalUpperY, 1, 1, 0, 0); }
    } else if (gameState == 1) {
        if (logicalLowerY >= 19 && logicalLowerY < 26) { drawBitmap(buffer, 16, 19, 7, 7, BigFont_G, logicalLowerY, 0, 1, 0, 0); drawBitmap(buffer, 24, 19, 7, 7, BigFont_A, logicalLowerY, 0, 1, 0, 0); drawBitmap(buffer, 32, 19, 7, 7, BigFont_M, logicalLowerY, 0, 1, 0, 0); drawBitmap(buffer, 40, 19, 7, 7, BigFont_E, logicalLowerY, 0, 1, 0, 0); }
        if (logicalUpperY >= 40 && logicalUpperY < 47) { drawBitmap(buffer, 16, 40, 7, 7, BigFont_O, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 24, 40, 7, 7, BigFont_V, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 32, 40, 7, 7, BigFont_E, logicalUpperY, 1, 1, 0, 0); drawBitmap(buffer, 40, 40, 7, 7, BigFont_R, logicalUpperY, 1, 1, 0, 0); }
    } else if (gameState == 2) {
        if (logicalLowerY >= 19 && logicalLowerY < 26) { drawBitmap(buffer, 16, 19, 7, 7, BigFont_G, logicalLowerY, 0, 0, 1, 0); drawBitmap(buffer, 24, 19, 7, 7, BigFont_A, logicalLowerY, 0, 0, 1, 0); drawBitmap(buffer, 32, 19, 7, 7, BigFont_M, logicalLowerY, 0, 0, 1, 0); drawBitmap(buffer, 40, 19, 7, 7, BigFont_E, logicalLowerY, 0, 0, 1, 0); }
        if (logicalUpperY >= 40 && logicalUpperY < 47) { drawBitmap(buffer, 12, 40, 7, 7, BigFont_C, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 20, 40, 7, 7, BigFont_L, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 28, 40, 7, 7, BigFont_E, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 36, 40, 7, 7, BigFont_A, logicalUpperY, 1, 0, 1, 0); drawBitmap(buffer, 44, 40, 7, 7, BigFont_R, logicalUpperY, 1, 0, 1, 0); }
    } else if (gameState == 3) {
        // [추가] Title Screen: PUSH S1
        if (logicalLowerY >= 19 && logicalLowerY < 26) {
            // PUSH
            drawBitmap(buffer, 16, 19, 7, 7, BigFont_P, logicalLowerY, 0, 1, 1, 1); // White
            drawBitmap(buffer, 24, 19, 7, 7, BigFont_U, logicalLowerY, 0, 1, 1, 1);
            drawBitmap(buffer, 32, 19, 7, 7, BigFont_S, logicalLowerY, 0, 1, 1, 1);
            drawBitmap(buffer, 40, 19, 7, 7, BigFont_H, logicalLowerY, 0, 1, 1, 1);
        }
        if (logicalUpperY >= 40 && logicalUpperY < 47) {
            // S1 (S + NumFont 1)
            drawBitmap(buffer, 26, 40, 7, 7, BigFont_S, logicalUpperY, 1, 1, 0, 0); // Red
            drawBitmap(buffer, 34, 41, 5, 5, NumFont[1], logicalUpperY, 1, 1, 0, 0);
        }
    }
}

void scanFrame() {
    HUB_OE_HI;
    for (uint8_t row = 0; row < 16; row++) {
        for (uint8_t plane = 0; plane < BCM_PLANES; plane++) {
            g_curBcmPlane = plane;
            HUB_OE_HI; delay_us(2);
            fillBuffer(bottomBuffer, row + 32, row + 48); fillBuffer(topBuffer, row, row + 16);
            for (int i = 0; i < 64; i++) { uint32_t data = bottomBuffer[i]; GPIOA->BSRR = (0x003F << 16) | (data & 0x003F); HUB_CLK_HI; HUB_CLK_LO; }
            for (int i = 0; i < 64; i++) { uint32_t data = topBuffer[i];    GPIOA->BSRR = (0x003F << 16) | (data & 0x003F); HUB_CLK_HI; HUB_CLK_LO; }
            GPIOB->BSRR = (0xF0000000) | ((row & 0x0F) << 12);
            HUB_LAT_HI; __asm("nop"); __asm("nop"); HUB_LAT_LO;
            HUB_OE_LO;
            uint32_t d = (uint32_t)BCM_BASE_DELAY << plane;
            delay_us(d);
            HUB_OE_HI;
        }
    }
    HUB_OE_HI;
}

// --- 게임 로직 ---
void spawnNaturalGoomba() {
    int count = 0;
    for(int i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active && goombas[i].natural) count++;
    if(count >= 2) return;
    for(int i=0; i<MAX_GOOMBAS; i++) if(!goombas[i].active) {
        goombas[i].active = 1; goombas[i].isSquashed = 0; goombas[i].natural = 1; goombas[i].dyingTimer = 0; goombas[i].onBox = 0;
        goombas[i].y = GROUND_Y - GOOMBA_H; goombas[i].vy = 0;
        if(rand() % 2 == 0) { goombas[i].x = -GOOMBA_W; goombas[i].vx = 1; }
        else { goombas[i].x = 64; goombas[i].vx = -1; }
        break;
    }
}

void spawnBoxGoomba(int x, int y, uint8_t isDestroyed) {
    for(int i=0; i<MAX_GOOMBAS; i++) if(!goombas[i].active) {
        goombas[i].active = 1; goombas[i].natural = 0; goombas[i].isSquashed = 0; goombas[i].dyingTimer = 0;
        goombas[i].vx = (rand()%2) ? 1 : -1; goombas[i].vy = 0;
        if (isDestroyed) { goombas[i].onBox = 0; goombas[i].x = x; goombas[i].y = y - 4; }
        else { goombas[i].onBox = 1; goombas[i].platformX = x; goombas[i].x = x; goombas[i].y = y - GOOMBA_H; }
        break;
    }
}

void createDebris(int x, int y) {
    for(int i=0; i<MAX_DEBRIS; i++) if(!debris[i].active) {
        debris[i].active = 1; debris[i].x = x + (rand() % BOX_W); debris[i].y = y; break;
    }
}

void spawnFallingBox() {
    for(int i=0; i<MAX_BOXES; i++) if(!boxes[i].active) {
        boxes[i].active = 1; boxes[i].isHit = 0; boxes[i].stopTimer = 0;
        boxes[i].x = rand()%(64-BOX_W); boxes[i].y = -BOX_H;
        int r = rand() % 10; boxes[i].itemType = (r<4)?0 : (r<7)?1 : 2;
        break;
    }
}

void applyItemEffect(uint8_t type) {
    if(type==0) {
        triggerCoinSound(); // 코인 소리
        coinCount++;
        if(coinCount>=10) {
            gameState=2;
            triggerGameClearSound(); // Game Clear 소리
        }
    } else if(type==1) {
        if(lives<5) lives++;
        else {
            triggerCoinSound(); // 코인 소리
            coinCount++;
            if(coinCount>=10) {
                gameState=2;
                triggerGameClearSound(); // Game Clear 소리
            }
        }
    }
}

void initGame() {
    marioX = 26; marioY = GROUND_Y - MARIO_H; marioDir = 0; lives = 2; coinCount = 0;
    invincibleTimer = 0; spawnTimer = 0;
    memset(goombas, 0, sizeof(goombas)); memset(boxes, 0, sizeof(boxes)); memset(debris, 0, sizeof(debris));
}

void hurtEffect() {
    if(invincibleTimer>0) return;

    triggerHurtSound(); // [추가] 삐빅 사운드

    lives--; invincibleTimer = 60;
    if(lives==0) {
        gameState=1;
        triggerGameOverSound(); // Game Over 소리
        for(int k=0; k<120; k++) {
            scanFrame();
            soundLogic();
        }
        // 게임 오버 후 다시 시작 화면(Title)으로 갈지, 바로 시작할지 결정
        // 여기서는 다시 Title 화면으로 가도록 설정
        gameState=3;
        initGame();
    }
}

void physicsStep() {
    static int frameCount = 0; frameCount++;
    soundLogic(); // 사운드 처리

    uint8_t key = readTM1650();

    // [추가] Start Screen 처리
    if (gameState == 3) {
        // S1 Key (Left key: 0x45) pressed -> Game Start
        if (key == 0x44) {
            gameState = 0;
            triggerCoinSound(); // 시작 확인음 (띠링~)
            initGame();
        }
        return; // Title 화면에서는 아래 게임 로직 실행 X
    }

    if(invincibleTimer > 0) invincibleTimer++;
    if(invincibleTimer > 120) invincibleTimer = 0;
    spawnTimer++;
    if (spawnTimer >= 200) { spawnTimer = 0; if (rand() % 100 < 70) spawnNaturalGoomba(); if (rand() % 100 < 50) spawnFallingBox(); }

    uint8_t k_l=(key==0x45), k_r=(key==0x55), k_j=(key==0x4C), k_rst=(key==0x5F);
    if(k_rst) {
        gameState = 3; // Reset to Title
        initGame(); return;
    }

    if(k_l) { if(marioX>0) marioX--; marioDir=1; if(frameCount%3==0) marioFrame^=1; }
    else if(k_r) { if(marioX<52) marioX++; marioDir=0; if(frameCount%3==0) marioFrame^=1; }
    else if(!isJumping) marioFrame = 0;

    if(k_j && !prevS2 && !isJumping) {
        isJumping=1; jumpPhase=0;
        triggerJumpSound(); // 점프 소리
    }
    prevS2 = k_j;

    if(isJumping) {
        if(frameCount%2==0) {
            marioY = (GROUND_Y - MARIO_H) + jumpArc[jumpPhase];
            if(jumpPhase > 8) {
                for(int i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active &&
                   !goombas[i].isSquashed && goombas[i].dyingTimer==0 &&
                   abs((marioX+6)-(goombas[i].x+6))<10 &&
                   abs((marioY+MARIO_H)-goombas[i].y)<=2) {

                    triggerSquashSound(); // 밟기 소리
                    triggerCoinSound();   // 코인 소리 (점수)

                    goombas[i].isSquashed=1; goombas[i].squashTimer=10;
                    coinCount++;
                    if(coinCount>=10) {
                        gameState=2;
                        triggerGameClearSound(); // Game Clear
                    }
                }
            }
            for(int i=0; i<MAX_BOXES; i++) if(boxes[i].active && !boxes[i].isHit &&
                abs((marioX+6)-(boxes[i].x+6))<10 && abs(marioY-(boxes[i].y+BOX_H))<6) {
                boxes[i].isHit=1; boxes[i].stopTimer=12;
                if(boxes[i].itemType==2) spawnBoxGoomba(boxes[i].x, boxes[i].y, 0);
                else applyItemEffect(boxes[i].itemType);
            }
            jumpPhase++; if(jumpPhase>=16) { isJumping=0; marioY=GROUND_Y-MARIO_H; }
        }
    } else marioY = GROUND_Y - MARIO_H;

    if(frameCount%2==0) {
        for(int i=0; i<MAX_BOXES; i++) if(boxes[i].active) {
            if(boxes[i].isHit) { if(boxes[i].stopTimer>0) boxes[i].stopTimer--; else boxes[i].active=0; }
            else {
                boxes[i].y++;
                if(boxes[i].y>=TOP_SCREEN_BOTTOM) {
                    if(boxes[i].itemType==2) spawnBoxGoomba(boxes[i].x, boxes[i].y, 1);
                    boxes[i].active=0; createDebris(boxes[i].x, boxes[i].y);
                }
            }
        }
        for(int i=0; i<MAX_DEBRIS; i++) if(debris[i].active) {
            debris[i].y+=2;
            if(invincibleTimer==0 && abs((marioX+6)-(debris[i].x+2))<8 && abs((marioY+8)-debris[i].y)<8) { debris[i].active=0; hurtEffect(); }
            if(debris[i].y>64) debris[i].active=0;
        }
        for(int i=0; i<MAX_GOOMBAS; i++) if(goombas[i].active) {
            if(goombas[i].dyingTimer>0) { goombas[i].dyingTimer--; if(goombas[i].dyingTimer==0) goombas[i].active=0; }
            else if(goombas[i].isSquashed) { if(goombas[i].squashTimer>0) goombas[i].squashTimer--; else goombas[i].active=0; }
            else {
                if(goombas[i].onBox) {
                    goombas[i].x+=goombas[i].vx;
                    if(goombas[i].x < goombas[i].platformX || goombas[i].x > goombas[i].platformX+BOX_W) goombas[i].onBox=0;
                } else {
                    if(goombas[i].y < GROUND_Y-GOOMBA_H) goombas[i].y+=2;
                    else {
                        goombas[i].y=GROUND_Y-GOOMBA_H; goombas[i].x+=goombas[i].vx;
                        if(goombas[i].vx>0 && goombas[i].x>64) goombas[i].active=0;
                        if(goombas[i].vx<0 && goombas[i].x<-12) goombas[i].active=0;
                    }
                }
                if(invincibleTimer==0 && !goombas[i].isSquashed && abs((marioX+6)-(goombas[i].x+6))<6 && abs((marioY+8)-(goombas[i].y+6))<8) {
                    hurtEffect(); goombas[i].dyingTimer=12;
                }
            }
        }
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_TIM4_Init();

  /* USER CODE BEGIN 2 */
  HUB_OE_HI;
  i2c_start(); i2c_write(0x48); i2c_write(0x01); i2c_stop();
  srand(HAL_GetTick());
  initGame();
  gameState = 3; // [중요] 게임 시작 시 대기 화면(Title)으로 설정

  uint32_t lastTick = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      for(int k=0; k<2; k++) scanFrame();
      if(HAL_GetTick() - lastTick > 33) {
          lastTick = HAL_GetTick();
          physicsStep();
      }
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider   = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider  = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider  = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

// 타이머 레지스터 직접 제어 초기화 (1MHz)
static void MX_TIM4_Init(void)
{
  __HAL_RCC_TIM4_CLK_ENABLE();

  // Timer Clock = 84MHz
  // Prescaler = 83 => 84MHz / 84 = 1MHz count clock
  TIM4->PSC = 83;
  TIM4->ARR = 1000; // 초기값 (의미 없음)

  // Update Interrupt Enable
  TIM4->DIER |= TIM_DIER_UIE;

  HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
// TIM4 인터럽트 핸들러 (Direct Register Access)
void TIM4_IRQHandler(void)
{
  if (TIM4->SR & TIM_SR_UIF) // Update Interrupt Flag 확인
  {
    TIM4->SR &= ~TIM_SR_UIF; // Flag Clear

    // PB8 Toggle (BSRR을 이용한 XOR 효과 구현은 복잡하므로 ODR 직접 제어)
    GPIOB->ODR ^= GPIO_PIN_8;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
