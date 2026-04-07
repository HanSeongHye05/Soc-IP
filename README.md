# 🏁 FPGA 기반 동계올림픽 멀티 워치 시스템 (Speed Skating 중심)

## 📌 프로젝트 개요

본 프로젝트는 FPGA 기반의 다기능 타이머 시스템으로,
동계올림픽 종목(스피드 스케이팅, 알파인 스키, 아이스하키, 시계 모드)을
하나의 시스템으로 통합하여 구현하였습니다.

특히, 저는 **Speed Skating 모드의 제어 로직을 Vitis(C)** 환경에서 구현하였으며,
Stopwatch IP를 기반으로 경기 기록을 처리하는 기능을 개발했습니다.

---

## 🎯 주요 기능

### ✔ Speed Skating (핵심 구현)

* Lap 기반 기록 측정 (총 4 Lap)
* Split Time 계산
* BEST 기록 자동 갱신
* 4 Lap 완료 시 자동 정지
* LCD에 Lap별 기록 및 BEST 표시

---

### ✔ 시스템 기능

* Switch를 통한 모드 전환
* LCD를 통한 종목 및 상태 출력
* FND(7-Segment)를 통한 시간 실시간 출력

---

## 🧠 시스템 구조

```
[Button / Switch Input]
        ↓
[Vitis (C) Control Logic]
        ↓ (AXI)
[Stopwatch IP (Verilog)]
        ↓
[LCD / FND Output]
```

---

## ⚙️ 하드웨어 구성 (Verilog IP)

* Stopwatch IP (시간 측정)
* FND Controller (7-Segment 출력)
* IIC Controller (LCD 제어)

※ Stopwatch IP는 팀 프로젝트에서 공동으로 개발된 모듈을 기반으로 사용하였습니다.

---

## 💻 소프트웨어 구성 (Vitis)

* Speed Skating 상태 로직 구현
* Lap split 및 BEST 기록 계산
* GPIO 기반 버튼 입력 처리
* AXI를 통한 IP 제어

---

## 🔥 핵심 구현 내용 (Speed Skating)

* 누적 시간 기반 Split 계산
* 이전 Lap 기준 차이 계산 방식 적용
* 최소 시간 기록(BEST) 자동 갱신
* LCD 부분 업데이트 최적화 (불필요한 전체 갱신 방지)

---

## 🧩 역할 분담

* **개인 구현**

  * Speed Skating 제어 로직 (Vitis)
  * Lap / BEST 기록 처리
  * LCD 출력 로직 일부

* **팀 공동 구현**

  * Stopwatch IP (Verilog)
  * 전체 시스템 통합

---

## 🚀 결과

* FPGA 기반 실시간 타이머 시스템 구현
* HW(IP) + SW(Vitis) 연동 구조 설계
* 멀티 모드 시스템 구현 경험 확보

---

## 📌 한 줄 요약

> Stopwatch IP를 기반으로 Speed Skating 경기 기록 로직을 Vitis에서 구현한 FPGA 제어 시스템

---
