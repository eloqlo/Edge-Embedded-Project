import socket
import json
import threading
import time
import random
import subprocess
import os

# --- 설정 (Qt 코드와 맞춰야 함) ---
TCP_PORT = 12345       # 명령/센서 데이터용
UDP_PORT = 5000        # 음성 데이터용
HOST = '0.0.0.0'       # 모든 접속 허용

# --- 1. 오디오 처리 (UDP 수신 -> 스피커 출력) ---
def audio_receiver():
    print(f"🎤 오디오 서버 시작 (UDP Port {UDP_PORT})")
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind((HOST, UDP_PORT))

    # 리눅스 'aplay' 명령어로 오디오 데이터를 파이프라인으로 넘김
    # 설정: Signed 16bit Little Endian, 8000Hz, Mono (Qt 설정과 동일)
    try:
        player = subprocess.Popen(
            ['aplay', '-f', 'S16_LE', '-r', '8000', '-c', '1', '-t', 'raw'],
            stdin=subprocess.PIPE
        )
    except FileNotFoundError:
        print("❌ 에러: 'aplay'가 없습니다. (sudo apt install alsa-utils)")
        return

    while True:
        try:
            data, addr = udp_sock.recvfrom(4096) # 데이터 받기
            if data:
                player.stdin.write(data) # 스피커로 쏘기
                player.stdin.flush()
        except Exception as e:
            print(f"Audio Error: {e}")

# --- 2. 센서 데이터 전송 (Telemetry) ---
def send_telemetry(conn):
    print("📡 센서 데이터 전송 시작...")
    while True:
        try:
            # 가짜 데이터 생성 (나중에 실제 센서 연결하세요)
            data = {
                "type": "TELEMETRY",
                "payload": {
                    "co_ppm": random.randint(0, 50),
                    "obstacle_cm": random.randint(10, 200),
                    "rollover": random.choice([True, False]) if random.random() > 0.9 else False
                }
            }
            message = json.dumps(data) + "\n"
            conn.sendall(message.encode())
            time.sleep(0.1) # 0.1초마다 전송
        except:
            break # 연결 끊기면 종료

# --- 3. 메인 로직 (TCP 연결 및 명령 수신) ---
def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, TCP_PORT))
    server.listen(1)

    print(f"🤖 시스템 대기중... (IP: {HOST}, Port: {TCP_PORT})")

    threading.Thread(target=audio_receiver, daemon=True).start()

    while True:
        conn, addr = server.accept()
        print(f"✅ 클라이언트 연결됨: {addr}")

        telemetry_thread = threading.Thread(target=send_telemetry, args=(conn,), daemon=True)
        telemetry_thread.start()

        try:
            with conn.makefile('r') as f:
                for line in f:
                    if not line.strip(): continue
                    print(f"📥 [RAW 데이터] {line.strip()}")

                    try:
                        request = json.loads(line)
                        if request['type'] == 'COMMAND':
                            process_command(request['payload'])
                    except json.JSONDecodeError:
                        print(f"⚠️ 깨진 데이터 수신: {line}")

        except Exception as e:
            print(f"❌ 연결 끊김: {e}")
        finally:
            conn.close()

# --- 4. 명령 처리 로직 ---
import socket
import json
import threading
import time
import random
import subprocess
import os

# --- 설정 (Qt 코드와 맞춰야 함) ---
TCP_PORT = 12345       # 명령/센서 데이터용
UDP_PORT = 5000        # 음성 데이터용
HOST = '0.0.0.0'       # 모든 접속 허용

# --- 1. 오디오 처리 (UDP 수신 -> 스피커 출력) ---
def audio_receiver():
    print(f"🎤 오디오 서버 시작 (UDP Port {UDP_PORT})")
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind((HOST, UDP_PORT))

    # 리눅스 'aplay' 명령어로 오디오 데이터를 파이프라인으로 넘김
    # 설정: Signed 16bit Little Endian, 8000Hz, Mono (Qt 설정과 동일)
    try:
        player = subprocess.Popen(
            ['aplay', '-f', 'S16_LE', '-r', '8000', '-c', '1', '-t', 'raw'],
            stdin=subprocess.PIPE
        )
    except FileNotFoundError:
        print("❌ 에러: 'aplay'가 없습니다. (sudo apt install alsa-utils)")
        return

    while True:
        try:
            data, addr = udp_sock.recvfrom(4096) # 데이터 받기
            if data:
                player.stdin.write(data) # 스피커로 쏘기
                player.stdin.flush()
        except Exception as e:
            print(f"Audio Error: {e}")

# --- 2. 센서 데이터 전송 (Telemetry) ---
def send_telemetry(conn):
    print("📡 센서 데이터 전송 시작...")
    while True:
        try:
            # 가짜 데이터 생성 (나중에 실제 센서 연결하세요)
            data = {
                "type": "TELEMETRY",
                "payload": {
                    "co_ppm": random.randint(0, 50),
                    "obstacle_cm": random.randint(10, 200),
                    "rollover": random.choice([True, False]) if random.random() > 0.9 else False
                }
            }
            message = json.dumps(data) + "\n"
            conn.sendall(message.encode())
            time.sleep(0.1) # 0.1초마다 전송
        except:
            break # 연결 끊기면 종료

# --- 3. 메인 로직 (TCP 연결 및 명령 수신) ---
def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, TCP_PORT))
    server.listen(1)

    print(f"🤖 시스템 대기중... (IP: {HOST}, Port: {TCP_PORT})")

    # 오디오 쓰레드 먼저 시작 (독립적으로 돎)
    threading.Thread(target=audio_receiver, daemon=True).start()

    while True:
        conn, addr = server.accept()
        print(f"✅ 클라이언트 연결됨: {addr}")

        # 센서 데이터 보내는 쓰레드 시작
        telemetry_thread = threading.Thread(target=send_telemetry, args=(conn,), daemon=True)
        telemetry_thread.start()

        # 명령(Command) 받는 반복문
        try:
            # 데이터를 줄 단위로 읽기 위해 파일 객체처럼 변환
            with conn.makefile('r') as f:
                for line in f:
                    if not line.strip(): continue
                    
                    try:
                        # JSON 파싱
                        request = json.loads(line)
                        if request['type'] == 'COMMAND':
                            process_command(request['payload'])
                    except json.JSONDecodeError:
                        print(f"⚠️ 깨진 데이터 수신: {line}")
                        
        except Exception as e:
            print(f"❌ 연결 끊김: {e}")
        finally:
            conn.close()

# --- 4. 명령 처리 로직 ---
def process_command(payload):
    target = payload.get('target')
    value = payload.get('value')
    action = payload.get('action')

    print(f"⚙️ 명령 처리: 타겟={target}, 값={value}")

    if target == 'DRIVE':
        if value == 'F': print("   ⬆️ 전진 (Forward)")
        elif value == 'B': print("   ⬇️ 후진 (Backward)")
        elif value == 'L': print("   ⬅️ 좌회전 (Left)")
        elif value == 'R': print("   ➡️ 우회전 (Right)")
        elif value == 'STOP': print("   🛑 정지 (Stop)")

    elif target == 'MIC':
        if value: print("   🎤 마이크 ON")
        else: print("   🔇 마이크 OFF")

    elif target == 'SYSTEM':
        if action == 'REBOOT': print("   ⚠️ 재부팅 시퀀스!")

if __name__ == "__main__":
    start_server()