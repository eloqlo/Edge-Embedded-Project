# 📄 JetDash 통신 프로토콜 명세서 (v1.0)

## 1. 개요 (Overview)
이 문서는 **라즈베리 파이(Vehicle)**와 **젯슨 나노(Control GUI)** 간의 데이터 통신 규약을 정의합니다.

| 항목 | 내용 | 비고 |
| :--- | :--- | :--- |
| **통신 방식** | TCP/IP Socket | |
| **포트 번호** | `12345` | 제어 및 센서 데이터 전용 |
| **인코딩** | UTF-8 | |
| **데이터 포맷** | JSON | |
| **구분자** | Line Feed (`\n`) | 메시지 끝에는 반드시 줄바꿈 포함 |

---

## 2. 공통 구조 (Common Packet Structure)
모든 송수신 메시지는 아래와 같은 최상위 JSON 구조를 가집니다.

```json
{
  "type": "MESSAGE_TYPE",   // 메시지 종류 (String)
  "timestamp": 1708301234,  // (선택) 보낸 시간 (Unix Timestamp, Double)
  "payload": { ... }        // 실제 데이터 객체 (Object)
}
