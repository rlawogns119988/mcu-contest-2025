# 2025 COSS 차세대 반도체 MCU 응용 경진대회

전남대학교 **팀 청소부** (김재훈 · 진광필 · 최민석) — 예선 통과 후 **본선 진출**

AVR(ATtiny85 / ATmega328P)과 STM32F4를 **HAL/Arduino 라이브러리 없이 레지스터 직접 제어**로
구현한 임베디드 펌웨어 모음입니다. 모든 코드는 C로 작성했습니다.

---

## 한눈에 보기

| 과제 | MCU | 핵심 구현 | 폴더 |
|---|---|---|---|
| 예선 — 화장실용 로봇청소기 | ATmega328P @16MHz | UART(HC-06 블루투스), 초음파 거리측정, 소프트웨어 PWM 주행 | [`01_preliminary_bathroom-cleaning-robot`](./01_preliminary_bathroom-cleaning-robot) |
| 본선 1일차 A — 스톱워치 / 시한폭탄 | ATtiny85 @1MHz | 소프트웨어 I2C(TM1650), Timer1 인터럽트 10ms 타이머, ADC 저항 분압 키패드 | [`02_final_day1_attiny85/A_stopwatch_timebomb`](./02_final_day1_attiny85/A_stopwatch_timebomb) |
| 본선 1일차 B — 호텔 금고(전자 도어락) | ATtiny85 @1MHz | TM1650 키 스캔 입력, 비밀번호 상태머신, 타임아웃/오입력 잠금 | [`02_final_day1_attiny85/B_hotel_safe`](./02_final_day1_attiny85/B_hotel_safe) |
| 본선 2일차 — HUB75 LED 매트릭스 게임 | ATmega328P / STM32F4 | HUB75 스캔 드라이버, BCM 밝기 제어, PROGMEM 스프라이트, 타이머 기반 사운드 | [`03_final_day2_hub75_mario`](./03_final_day2_hub75_mario) |

---

## 이 저장소에서 다룬 임베디드 SW 기술

- **레지스터 직접 제어** — `DDRx/PORTx/PINx`, `GPIOx->BSRR` 를 직접 조작. HAL_GPIO_WritePin 같은
  래퍼는 HUB75 픽셀 클럭 속도를 맞출 수 없어 배제
- **소프트웨어 I2C (bit-banging)** — ATtiny85에는 하드웨어 I2C가 없어 START/STOP/WRITE/READ를
  직접 구현하여 TM1650 디스플레이·키패드 제어
- **타이머 & 인터럽트** — Timer1 CTC로 10ms 틱 생성(`ISR(TIMER1_COMPA_vect)`),
  STM32는 TIM4 (PSC=83 → 1MHz 카운트 클럭) 인터럽트로 사각파 사운드 생성
- **ADC 활용** — GPIO 6개뿐인 ATtiny85에서 저항(0Ω/2.2kΩ/10kΩ) 분압으로
  **핀 1개에 버튼 3개**를 매핑하고 ADC 값 구간으로 판별
- **소프트웨어 PWM** — Timer2 기반 ~1.95kHz PWM으로 H-브릿지 모터 속도를 자동/수동 모드에서 동일하게 유지
- **디스플레이 드라이빙** — HUB75 상/하 패널 동시 스캔, BCM(Binary Code Modulation) 3플레인 밝기 제어
- **메모리 제약 대응** — 스프라이트를 `PROGMEM` 에 두어 SRAM 절약, 점프 궤적을 룩업테이블로 사전 계산해 연산량 감소
- **통신** — UART 레지스터 직접 제어(9600-8N1)로 블루투스 원격 조종 프로토콜 구현

## 개발 환경

- Microchip Studio (AVR-GCC), Arduino IDE, STM32CubeIDE / CubeMX
- avrdudess (ATtiny85 fuse 설정: 내부 1MHz 사용을 위해 `L=0x62`), ST-LINK
- 계측: 오실로스코프 / 로직 애널라이저로 I2C·PWM 파형 검증

## 저장소 구조

```
MCU-Contest-2025/
├── 01_preliminary_bathroom-cleaning-robot/   예선 출품작
├── 02_final_day1_attiny85/                   본선 1일차 (ATtiny85)
│   ├── A_stopwatch_timebomb/
│   └── B_hotel_safe/
├── 03_final_day2_hub75_mario/                본선 2일차 (ATmega328P / STM32F4)
└── docs/                                     공용 하드웨어 모듈 회로도
```

## 시연 영상

<!-- 유튜브에 '미등록(Unlisted)'으로 올린 뒤 아래 링크를 채우세요 -->
- 예선 로봇청소기 동작 영상: (링크)
- 본선 1일차 A/B 동작 영상: (링크)
- 본선 2일차 Arduino / STM32 동작 영상: (링크)
