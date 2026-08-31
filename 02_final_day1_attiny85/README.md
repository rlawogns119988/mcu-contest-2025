# 본선 1일차 — ATtiny85 과제

**ATtiny85 @ 1MHz (내부 오실레이터, fuse L=0x62)** 환경에서 진행한 과제입니다.

ATtiny85는 **GPIO가 6개(PB0~PB5, RESET 제외 시 5개)** 뿐이고 **하드웨어 I2C가 없습니다.**
따라서 두 과제 모두 아래 두 가지가 핵심 제약이었습니다.

1. TM1650 4-digit 디스플레이 제어를 위한 **소프트웨어 I2C(bit-banging)** 직접 구현
2. 부족한 핀으로 다수 입력을 받기 위한 **핀 다중화 설계**

- [`A_stopwatch_timebomb`](./A_stopwatch_timebomb) — 스톱워치 + 시한폭탄 (모드 전환형)
- [`B_hotel_safe`](./B_hotel_safe) — 호텔 금고(전자 도어락)
