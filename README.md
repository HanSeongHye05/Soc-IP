동계올림픽 멀티 기능 임베디드 시스템 (FPGA + Vitis)

1. 프로젝트 개요

본 프로젝트는 FPGA 기반 Custom IP와 Vitis 소프트웨어를 이용하여
동계올림픽 종목별 시간 측정 기능을 하나의 시스템으로 통합 구현한 것이다.

단순 타이머가 아니라 종목별 요구사항에 맞게 기능을 분리하여
하드웨어와 소프트웨어를 나누어 설계하였다.


2. 시스템 구성

- 프로세서: MicroBlaze
- 개발 환경: Vivado, Vitis

- Custom IP
  - Stopwatch IP
  - Cooktimer IP
  - Watch IP
  - FND Controller
  - LCD Controller (I2C)
  - Alarm IP

- 주변 장치
  - GPIO (버튼, 스위치)
  - UART
  - LCD 2개
  - 7-Segment(FND)


3. 주요 기능

(1) 스피드 스케이팅

- Stopwatch IP를 이용한 시간 측정
- 4바퀴 Lap 기록 저장
- 각 Lap의 Split Time을 소프트웨어에서 계산
  (현재 시간 - 이전 Lap 시간)

- 가장 빠른 기록(BEST) 자동 갱신
- LCD에 Lap 기록 및 BEST 표시

설계 특징
- Lap 기능을 IP가 아닌 소프트웨어에서 처리하여 동작 충돌 방지
- LCD 전체 갱신이 아닌 일부 영역만 업데이트하여 화면 깨짐 방지


(2) 알파인 스키

- Stopwatch 기반 기록 측정
- 버튼 입력을 통해 패널티 시간(+3초) 누적

설계 특징
- 실제 경기 규칙을 반영하여 시간 + 패널티를 합산 출력


(3) 아이스하키

- FSM 기반 경기 진행 제어

상태 구성
- 경기 시간 (Period)
- 작전 시간 (Timeout)
- 휴식 시간 (Intermission)
- 대기 상태 (Idle)

주요 기능
- 상태 자동 전환
- 타이머 일시정지 및 재시작
- 경기 종료 자동 처리

설계 특징
- Cooktimer IP와 상태 머신을 결합하여 구현


(4) 시계 기능

- 서울 / 밀라노 시간 동시 표시
- 버튼을 이용한 시간 조정

설계 특징
- 시차 계산을 통해 두 지역 시간 동시 출력


4. 설계 방식

(1) Hardware / Software 분리

- 시간 카운팅: Hardware(IP)
- 모드 제어 및 로직: Software
- 사용자 인터페이스(LCD, FND): Software

시간 정확도가 중요한 부분은 IP로 구현하고,
제어 및 표시 로직은 소프트웨어에서 처리하도록 분리하였다.


(2) 인터럽트 기반 처리

- GPIO 인터럽트를 이용한 버튼 입력 처리
- UART 인터럽트를 이용한 데이터 수신 처리

Polling 방식이 아닌 이벤트 기반으로 설계하였다.


(3) LCD 출력 방식

- 전체 갱신이 아닌 필요한 부분만 업데이트
- 화면 깨짐 및 깜빡임 문제를 최소화


5. 프로젝트 구조

vitis/
 ├── basic_version/
 │   └── main.c
 │
 └── extended_version/
     └── main.c


6. 실행 방법

1) Vivado에서 Bitstream 생성
2) Hardware Export (XSA)
3) Vitis에서 프로젝트 생성
4) Build 후 FPGA 보드에 다운로드


7. 배운 점

- Hardware와 Software를 나누어 설계하는 방법
- 인터럽트 기반 임베디드 시스템 구현
- FSM을 이용한 상태 제어
- LCD 및 FND를 이용한 출력 제어
- 실제 요구사항을 반영한 시스템 설계 경험


8. 개선 방향

- Digital Twin(Blender + Unity) 연동
- CAN 통신 기반 확장
- GUI 기반 모니터링 기능 추가
