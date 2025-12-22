# JetDash 통신 프로토콜 명세서 (v1.0)

## 1. 개요 (Overview)
이 문서는 라즈베리 파이(Vehicle)와 로컬 컴퓨터(Control GUI) 간의 데이터 통신 규약을 정의합니다.

| 항목 | 내용 | 비고 |
| :--- | :--- | :--- |
| **통신 방식** | TCP/IP Socket | |
| **포트 번호** | `12345` | 제어 및 센서 데이터 전용 |
| **인코딩** | UTF-8 | |
| **데이터 포맷** | JSON | |
| **구분자** | Line Feed (`\n`) | 메시지 끝에는 반드시 줄바꿈 포함 |

---

## 2. 공통 구조 (Common Packet Structure)


### 2.1 공통 구조 (Common Structure)
모든 송수신 메시지는 아래와 같은 최상위 JSON 구조를 가집니다.
```json
{
  "type": "MESSAGE_TYPE",   // "COMMAND" 또는 "TELEMETRY"
  "payload": { ... }        // 실제 데이터 객체
}
```

### 2.2 제어 명령 (Client to Server)
* **Type:** `"COMMAND"`
* **설명:** Qt 클라이언트(Jetson/PC)에서 라즈베리 파이의 동작을 제어하기 위해 전송합니다.

#### A. 주행 제어 (Drive Control)
* 키보드 `WASD` 입력 및 해제(Release) 이벤트에 매핑됩니다.

| Target | Value | 설명 | 키 매핑 |
| :--- | :--- | :--- | :--- |
| `DRIVE` | `"F"` | 전진 (Forward) | `W` 누름 |
| `DRIVE` | `"B"` | 후진 (Backward) | `S` 누름 |
| `DRIVE` | `"L"` | 좌회전 (Left) | `A` 누름 |
| `DRIVE` | `"R"` | 우회전 (Right) | `D` 누름 |
| `DRIVE` | `"STOP"` | 모터 정지 | 키 뗌 (Release) |

**[JSON 예시]**
```json
{
  "type": "COMMAND",
  "payload": { "target": "DRIVE", "value": "F" }
}
```
#### B. 기능 제어 (Features)
* UI 버튼(토글) 입력에 매핑됩니다.

| Target | Value (Type) | 설명 |
| :--- | :--- | :--- |
| `MIC` | `true` / `false` | 마이크 스트리밍 시작(ON)/종료(OFF) 알림 |
| `OBJECT_DETECTION` | `true` / `false` | AI 객체 탐지 기능 활성화/비활성화 |

**[JSON 예시]**
```json
{
  "type": "COMMAND",
  "payload": { "target": "MIC", "value": true }
}
```

#### C. 시스템 제어 (System)
* 차량 시스템에 대한 관리자 명령입니다.

| Target | Value | 설명 |
| :--- | :--- | :--- |
| `SYSTEM` | `"REBOOT"` | 라즈베리 파이 시스템 재시작 (`sudo reboot`) |

**[JSON 예시]**
```json
{
  "type": "COMMAND",
  "payload": { "target": "SYSTEM", "value": "REBOOT" }
}
```

### 2.3 센서 데이터 (Server to Client)
* **Type:** `"TELEMETRY"`
* **주기:** 약 10Hz (0.1초 간격) 권장
* **설명:** 라즈베리 파이가 수집한 센서 정보를 클라이언트로 전송합니다.

| Key | Type | Unit | 설명 |
| :--- | :--- | :--- | :--- |
| `co_ppm` | Int | ppm | 일산화탄소 센서 측정값 |
| `obstacle_cm` | Int | cm | 초음파 센서 거리값 (7cm 미만 경고) |
| `rollover` | Bool | - | 전복 감지 여부 (`true` = 위험/전복됨) |

**[JSON 예시]**
```json
{
  "type": "TELEMETRY",
  "payload": {
    "co_ppm": 12,
    "obstacle_cm": 150,
    "rollover": false
  }
}
