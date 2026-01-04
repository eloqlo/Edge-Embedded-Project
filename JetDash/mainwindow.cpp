#include "mainwindow.h"
#include <QGridLayout>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QStyle>
#include <QMediaDevices>
#include <QAudioDevice>

// ★ 설정: 라즈베리 파이 주소 및 포트
const QString RPI_IP = "100.102.180.32";
const int PORT_CMD = 12345;  // TCP (명령/센서)
const int PORT_AUDIO = 5000; // UDP (음성)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    applyStyles();

    // ---------------------------------------------------------
    // 1. TCP 소켓 설정 (명령 및 센서 데이터)
    // ---------------------------------------------------------
    tcpSocket = new QTcpSocket(this);

    // 데이터가 들어오면 읽기 함수 실행
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::readSensorData);

    // 연결 상태 모니터링
    connect(tcpSocket, &QTcpSocket::connected, this, [this](){
        qDebug() << "Link Status: CONNECTED";
        lblSystemStatus->setText("System : <font color='#2ecc71'>Connected</font>");
    });

    connect(tcpSocket, &QTcpSocket::disconnected, this, [this](){
        qDebug() << "Link Status: DISCONNECTED";
        lblSystemStatus->setText("System : <font color='red'>Disconnected</font>");
    });

    // ---------------------------------------------------------
    // 2. 자동 재접속 타이머 (3초마다 체크)
    // ---------------------------------------------------------
    reconnectTimer = new QTimer(this);
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::attemptConnection);
    reconnectTimer->start(3000); // 3000ms = 3초

    // 프로그램 시작 시 1회 즉시 시도
    attemptConnection();

    // ---------------------------------------------------------
    // 3. UDP 소켓 (음성 전송용)
    // ---------------------------------------------------------
    udpSocket = new QUdpSocket(this);

    // 4. 오디오 설정 (수정됨: Int16 강제 고정)
    QAudioDevice info = QMediaDevices::defaultAudioInput();
    QAudioFormat format = info.preferredFormat();

    // ★★★ 여기가 핵심입니다! ★★★
    // 마이크가 Float를 좋아하든 말든, 우리는 Int16으로 받아야 계산이 됩니다.
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelCount(1);

    // 샘플 레이트는 마이크가 좋아하는 거 씁니다 (보통 48000Hz)
    // (만약 8000Hz가 지원되면 8000으로 바꾸셔도 됩니다)

    qDebug() << "🎤 설정된 포맷:" << format.sampleFormat();
    qDebug() << "🎧 설정된 주파수:" << format.sampleRate();

    audioInput = new QAudioSource(info, format, this);

    // ---------------------------------------------------------
    // 5. 버튼 이벤트 연결
    // ---------------------------------------------------------

    // (1) 마이크 토글 버튼 연결 부분
    connect(btnMicToggle, &QPushButton::toggled, this, [this](bool checked){

        // ★ [추가] 마이크 상태(checked)에 따라 슬라이더 잠금/해제
        // checked가 true면 활성화, false면 비활성화(회색) 됩니다.
        volumeSlider->setEnabled(checked);

        if(checked) {
            // [ON]
            btnMicToggle->setText("MIC ON (Streaming)");
            sendJsonCommand("MIC", true);

            audioDevice = audioInput->start();
            connect(audioDevice, &QIODevice::readyRead, this, &MainWindow::processAudio);
            qDebug() << "Audio Streaming STARTED";
        } else {
            // [OFF]
            btnMicToggle->setText("🎤 Mic OFF"); // 텍스트도 원래대로 깔끔하게
            sendJsonCommand("MIC", false);

            audioInput->stop();
            if(audioDevice) audioDevice->disconnect(this);

            // ★ [추가] 껐을 때 게이지 바가 멈춰있으면 보기 싫으니 0으로 초기화
            volumeBar->setValue(0);

            qDebug() << "Audio Streaming STOPPED";
        }

        // 스타일 갱신 (빨간색/회색 바뀌게)
        btnMicToggle->style()->unpolish(btnMicToggle);
        btnMicToggle->style()->polish(btnMicToggle);
    });

    // (2) 객체 탐지 토글
    // ---------------------------------------------------------
    // (새로 추가) RGB 탐지 버튼 연결
    // ---------------------------------------------------------
    connect(btnRgbDetect, &QPushButton::toggled, this, [this](bool checked){
        if(checked) {
            btnRgbDetect->setText("RGB Detect ON");
            // 프로토콜: RGB 탐지만 켜라
            sendJsonCommand("DETECT_RGB", true);
        } else {
            btnRgbDetect->setText("RGB Detect OFF");
            sendJsonCommand("DETECT_RGB", false);
        }
    });

    // ---------------------------------------------------------
    // (새로 추가) 열화상 탐지 버튼 연결
    // ---------------------------------------------------------
    connect(btnThermalDetect, &QPushButton::toggled, this, [this](bool checked){
        if(checked) {
            btnThermalDetect->setText("Thermal Detect ON");
            // 프로토콜: 열화상 탐지만 켜라
            sendJsonCommand("DETECT_THERMAL", true);
        } else {
            btnThermalDetect->setText("Thermal Detect OFF");
            sendJsonCommand("DETECT_THERMAL", false);
        }
    });

    // (3) 시스템 재부팅
    connect(btnReboot, &QPushButton::clicked, this, [this](){
        sendJsonCommand("SYSTEM", "REBOOT");
    });
}

MainWindow::~MainWindow()
{
    if(tcpSocket->isOpen()) tcpSocket->close();
}

// [슬롯] 자동 재접속 시도
void MainWindow::attemptConnection()
{
    // 연결이 끊겨 있을 때만 시도
    if (tcpSocket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "Attempting to connect to" << RPI_IP << "...";
        lblSystemStatus->setText("System : <font color='#e67e22'>Reconnecting...</font>");
        tcpSocket->connectToHost(RPI_IP, PORT_CMD);
    }
}

void MainWindow::processAudio()
{
    // [1] 안전 장치: 장치가 없거나 데이터가 없으면 종료
    if (!audioDevice) return;

    // 데이터를 몽땅 읽어옵니다.
    QByteArray data = audioDevice->readAll();

    // 읽은 데이터가 비어있으면 할 게 없으니 리턴
    if (data.isEmpty()) return;

    // -----------------------------------------------------------
    // [2] 데이터 처리 (볼륨 조절 + 시각화)
    // -----------------------------------------------------------

    // QByteArray(바이트 덩어리)를 16비트 정수 배열(숫자 덩어리)로 변환
    // Int16: -32768 ~ +32767 사이의 숫자
    int16_t *samples = (int16_t *)data.data();
    int sampleCount = data.size() / 2; // 2바이트가 숫자 1개이므로 개수는 절반

    int maxAmplitude = 0; // 이번 턴에서 가장 큰 소리 크기 (게이지바 용)

    for (int i = 0; i < sampleCount; ++i) {
        int originalSample = samples[i];

        // (A) 볼륨 증폭 (현재 게인값 곱하기)
        // currentGain이 1.0이면 원본, 2.0이면 2배
        int amplifiedSample = static_cast<int>(originalSample * currentGain);

        // (B) 클리핑 방지 (소리가 너무 커서 찢어지는 현상 막기)
        if (amplifiedSample > 32767) amplifiedSample = 32767;
        if (amplifiedSample < -32768) amplifiedSample = -32768;

        // (C) 변경된 값을 다시 데이터에 덮어쓰기
        samples[i] = static_cast<int16_t>(amplifiedSample);

        // (D) 가장 큰 소리 찾기 (게이지바 그리기 위해 절댓값 사용)
        int absValue = std::abs(amplifiedSample);
        if (absValue > maxAmplitude) {
            maxAmplitude = absValue;
        }
    }

    // -----------------------------------------------------------
    // [3] UI 업데이트 (초록색 바)
    // -----------------------------------------------------------
    // 0 ~ 32768 범위를 0 ~ 100 퍼센트로 변환
    int percentage = (maxAmplitude * 100) / 32768;

    // 너무 작으면 0으로 표시 (노이즈 무시)
    if (percentage < 2) percentage = 0;

    volumeBar->setValue(percentage);

    // -----------------------------------------------------------
    // [4] UDP 전송
    // -----------------------------------------------------------
    // 볼륨 조절이 완료된 data를 라즈베리 파이로 쏘기
    udpSocket->writeDatagram(data, QHostAddress(RPI_IP), PORT_AUDIO);
}

// JSON 명령 전송 도우미
void MainWindow::sendJsonCommand(QString target, QJsonValue value)
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Failed to send command: Not Connected.";
        return;
    }

    QJsonObject payload;
    payload["target"] = target;
    payload["value"] = value;

    QJsonObject json;
    json["type"] = "COMMAND";
    json["payload"] = payload;

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    tcpSocket->write(data + "\n");
    tcpSocket->flush(); // 즉시 전송 강제

    // 디버그 출력
    qDebug().noquote() << "[SENT]" << data;
}

// 키보드 누름 (주행 시작)
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(!event->isAutoRepeat()) {
        switch(event->key()) {
        case Qt::Key_W: sendJsonCommand("DRIVE", "F"); break;
        case Qt::Key_S: sendJsonCommand("DRIVE", "B"); break;
        case Qt::Key_A: sendJsonCommand("DRIVE", "L"); break;
        case Qt::Key_D: sendJsonCommand("DRIVE", "R"); break;
        }
    }
}

// 키보드 뗌 (주행 정지)
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if(!event->isAutoRepeat()) {
        switch(event->key()) {
        case Qt::Key_W:
        case Qt::Key_S:
        case Qt::Key_A:
        case Qt::Key_D:
            sendJsonCommand("DRIVE", "STOP");
            break;
        }
    }
}

// 센서 데이터 수신 및 파싱
void MainWindow::readSensorData() {
    while (tcpSocket->canReadLine()) {
        QByteArray data = tcpSocket->readLine();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if (jsonDoc.isNull()) continue;

        QJsonObject jsonObj = jsonDoc.object();
        if (jsonObj["type"].toString() == "TELEMETRY") {
            QJsonObject payload = jsonObj["payload"].toObject();

            int co = payload["co_ppm"].toInt();
            int dist = payload["obstacle_cm"].toInt();
            bool isRollover = payload["rollover"].toBool();

            // CO 농도 표시
            lblCO->setText(QString("CO Level : <font color='#ff5252'>%1 ppm</font>").arg(co));

            // 거리 표시 (30cm 미만 경고)
            if(dist < 30) {
                lblDistance->setText(QString("Distance : <font color='red'>WARNING %1cm</font>").arg(dist));
            } else {
                lblDistance->setText(QString("Distance : <font color='#ffb142'>%1cm</font>").arg(dist));
            }

            // 전복 여부 표시 (이모티콘 없이 색상으로만 구분)
            if (isRollover) {
                if (!lblRollover->text().contains("DANGER")) {
                    lblRollover->setText("Rollover : <font color='red'>DANGER</font>");
                    rgbCameraLabel->setStyleSheet("border: 5px solid red; background-color: #300000; color: white;");
                }
            } else {
                if (!lblRollover->text().contains("Safe")) {
                    lblRollover->setText("Rollover : <font color='#00d2d3'>Safe</font>");
                    rgbCameraLabel->setStyleSheet("border: 3px solid #ff5252; color: #ff5252; font-weight: bold; background-color: black; border-radius: 8px;");
                }
            }
            lblSystemStatus->setText("System : <font color='#2ecc71'>Connected (Receiving)</font>");
        }
    }
}

void MainWindow::setupUi() {
    // ---------------------------------------------------------
    // 1. [스타일] 전체 테마 설정 (CSS 문법)
    // ---------------------------------------------------------
    this->setStyleSheet(
        "QMainWindow { background-color: #2b2b2b; }" // 전체 배경: 진한 회색
        "QLabel { color: #ecf0f1; font-family: 'Segoe UI', sans-serif; }" // 글자: 흰색

        // 버튼 스타일 (평소)
        "QPushButton { "
        "  background-color: #34495e; color: white; border: none; "
        "  border-radius: 5px; padding: 10px; font-weight: bold;"
        "}"
        // 버튼 스타일 (마우스 올렸을 때)
        "QPushButton:hover { background-color: #4a698a; }"
        // 버튼 스타일 (켜졌을 때/Checked)
        "QPushButton:checked { background-color: #e74c3c; }" // 빨간색 강조

        // 그룹박스/프레임 스타일
        "QFrame#MonitorFrame { background-color: #000000; border: 2px solid #555; border-radius: 10px; }"
        "QFrame#ControlPanel { background-color: #3a3a3a; border-radius: 10px; }"
        );

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 전체 레이아웃: 수직 (위: 카메라 / 아래: 컨트롤)
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ---------------------------------------------------------
    // 2. [상단] 카메라 모니터 영역
    // ---------------------------------------------------------
    QWidget *monitorContainer = new QWidget(this);
    QHBoxLayout *monitorLayout = new QHBoxLayout(monitorContainer);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(15);

    // ---------------------------------------------------------
    // [상단] 카메라 모니터 영역 (수정됨)
    // ---------------------------------------------------------

    // (1) RGB 카메라 프레임
    QFrame *rgbFrame = new QFrame(this);
    rgbFrame->setObjectName("MonitorFrame");
    QVBoxLayout *rgbLayout = new QVBoxLayout(rgbFrame);
    rgbLayout->setContentsMargins(10, 10, 10, 10); // 여백 좀 줌

    rgbCameraLabel = new QLabel("RGB CAMERA\n[NO SIGNAL]", this);
    rgbCameraLabel->setAlignment(Qt::AlignCenter);
    rgbCameraLabel->setStyleSheet("color: #7f8c8d; font-weight: bold;");

    // ★ [추가] RGB 탐지 버튼 (화면 바로 아래)
    btnRgbDetect = new QPushButton("RGB Detect OFF", this);
    btnRgbDetect->setCheckable(true);
    btnRgbDetect->setCursor(Qt::PointingHandCursor);
    btnRgbDetect->setFixedHeight(30); // 얇고 세련되게
    btnRgbDetect->setStyleSheet(
        "QPushButton { background-color: #2c3e50; border: 1px solid #555; font-size: 12px; }"
        "QPushButton:checked { background-color: #27ae60; border: 1px solid #2ecc71; }"
        );

    rgbLayout->addWidget(rgbCameraLabel);
    rgbLayout->addWidget(btnRgbDetect); // 라벨 밑에 추가


    // (2) 열화상 카메라 프레임
    QFrame *thermalFrame = new QFrame(this);
    thermalFrame->setObjectName("MonitorFrame");
    QVBoxLayout *thermalLayout = new QVBoxLayout(thermalFrame);
    thermalLayout->setContentsMargins(10, 10, 10, 10);

    thermalCameraLabel = new QLabel("THERMAL\n[NO SIGNAL]", this);
    thermalCameraLabel->setAlignment(Qt::AlignCenter);
    thermalCameraLabel->setStyleSheet("color: #7f8c8d; font-weight: bold;");

    // ★ [추가] 열화상 탐지 버튼
    btnThermalDetect = new QPushButton("Thermal Detect OFF", this);
    btnThermalDetect->setCheckable(true);
    btnThermalDetect->setCursor(Qt::PointingHandCursor);
    btnThermalDetect->setFixedHeight(30);
    btnThermalDetect->setStyleSheet(
        "QPushButton { background-color: #2c3e50; border: 1px solid #555; font-size: 12px; }"
        "QPushButton:checked { background-color: #e67e22; border: 1px solid #d35400; }" // 주황색 맛
        );

    thermalLayout->addWidget(thermalCameraLabel);
    thermalLayout->addWidget(btnThermalDetect); // 라벨 밑에 추가

    // 모니터 배치
    monitorLayout->addWidget(rgbFrame);
    monitorLayout->addWidget(thermalFrame);

    // 상단 영역 비율 (화면의 60% 차지)
    mainLayout->addWidget(monitorContainer, 6);


    // ---------------------------------------------------------
    // 3. [하단] 컨트롤 패널 (센서 + 버튼)
    // ---------------------------------------------------------
    QFrame *controlPanel = new QFrame(this);
    controlPanel->setObjectName("ControlPanel"); // 배경색 들어간 패널
    QHBoxLayout *panelLayout = new QHBoxLayout(controlPanel);
    panelLayout->setContentsMargins(20, 20, 20, 20);
    panelLayout->setSpacing(20);

    // (A) 왼쪽: 센서 데이터 (텍스트)
    QVBoxLayout *sensorLayout = new QVBoxLayout();
    lblCO = new QLabel("CO Level : 0 ppm", this);
    lblCO->setStyleSheet("font-size: 14px; color: #f1c40f;"); // 노란색 강조

    lblRollover = new QLabel("Rollover : SAFE", this);
    lblRollover->setStyleSheet("font-size: 14px; color: #2ecc71;"); // 초록색 강조

    lblDistance = new QLabel("Distance : - cm", this);
    lblSystemStatus = new QLabel("System : Ready", this);

    sensorLayout->addWidget(lblCO);
    sensorLayout->addWidget(lblRollover);
    sensorLayout->addWidget(lblDistance);
    sensorLayout->addWidget(lblSystemStatus);
    sensorLayout->addStretch(); // 위로 밀착

    // (B) 오른쪽: 버튼 및 슬라이더 뭉치
    QVBoxLayout *actionLayout = new QVBoxLayout();
    actionLayout->setSpacing(10);

    // 버튼 2: 마이크
    btnMicToggle = new QPushButton("🎤 Mic OFF", this);
    btnMicToggle->setCheckable(true);
    btnMicToggle->setFixedHeight(40); // ★ 높이 고정
    btnMicToggle->setCursor(Qt::PointingHandCursor);

    // 버튼 3: 재부팅 (빨간 맛)
    btnReboot = new QPushButton("⚠️ System Reboot", this);
    btnReboot->setFixedHeight(40); // ★ 높이 고정
    btnReboot->setCursor(Qt::PointingHandCursor);
    // 재부팅 버튼만 특별하게 스타일 덮어쓰기
    btnReboot->setStyleSheet("background-color: #c0392b; color: white; border-radius: 5px;");

    // 슬라이더 & 바 (마이크 버튼 아래에 배치)
    QWidget *sliderBox = new QWidget(this);
    QVBoxLayout *sliderLayout = new QVBoxLayout(sliderBox);
    sliderLayout->setContentsMargins(0,0,0,0);

    volumeBar = new QProgressBar(this);
    volumeBar->setFixedHeight(6); // 아주 얇게
    volumeBar->setTextVisible(false);
    volumeBar->setStyleSheet("QProgressBar { background: #222; border-radius: 3px; } QProgressBar::chunk { background: #00e676; border-radius: 3px; }");

    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 200);
    volumeSlider->setValue(100);
    volumeSlider->setToolTip("Mic Gain Boost");

    // 슬라이더 핸들 좀 예쁘게 (CSS)
    volumeSlider->setStyleSheet(
        // ------------------------------------------------
        // 1. [활성 상태] (MIC ON 일 때) - 형광 초록 & 흰색
        // ------------------------------------------------
        "QSlider::groove:horizontal { "
        "    height: 6px; "
        "    background: #444444; "
        "    border-radius: 3px; "
        "}"
        "QSlider::handle:horizontal { "
        "    background: white; "        // 손잡이는 흰색
        "    width: 16px; "
        "    height: 16px; "
        "    margin: -5px 0; "
        "    border-radius: 8px; "
        "}"
        "QSlider::sub-page:horizontal { "
        "    background: #00e676; "      // 채워진 곳은 형광 초록!
        "    border-radius: 3px; "
        "}"

        // ------------------------------------------------
        // 2. [비활성 상태] (MIC OFF 일 때) - 전부 칙칙한 회색
        // ------------------------------------------------
        "QSlider::groove:horizontal:disabled { "
        "    background: #2b2b2b; "      // 트랙: 배경이랑 비슷하게 숨김
        "}"
        "QSlider::handle:horizontal:disabled { "
        "    background: #555555; "      // 손잡이: 어두운 회색 (클릭 못하게 생김)
        "}"
        "QSlider::sub-page:horizontal:disabled { "
        "    background: #3a3a3a; "      // 채워진 곳: 형광색 뺌
        "}"
        );
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value){
        currentGain = value / 100.0f;
    });

    volumeSlider->setEnabled(false);

    // 오른쪽 레이아웃에 차곡차곡 쌓기
    actionLayout->addWidget(btnMicToggle);
    actionLayout->addWidget(volumeBar); // 마이크 버튼 바로 아래 게이지
    actionLayout->addWidget(volumeSlider); // 그 아래 슬라이더
    actionLayout->addSpacing(10); // 약간 띄우고
    actionLayout->addWidget(btnReboot);
    actionLayout->addStretch();

    // 패널에 왼쪽(센서), 오른쪽(버튼) 담기
    panelLayout->addLayout(sensorLayout, 1); // 1:1 비율 아님, 센서는 좁게
    panelLayout->addLayout(actionLayout, 2); // 버튼 쪽을 좀 더 넓게

    // 메인 레이아웃에 하단 패널 추가 (비율 4)
    mainLayout->addWidget(controlPanel, 4);
}

// 스타일 시트 적용 (CSS)
void MainWindow::applyStyles() {
    // R"css(...)css" 문법 사용 (에러 방지)
    QString style = R"css(
        QMainWindow { background-color: #2b2b2b; }
        QWidget { color: #ecf0f1; font-family: 'Arial', sans-serif; font-size: 16px; }
        QLabel { background-color: #000000; border-radius: 8px; }
        QFrame { background-color: #1e1e1e; border: 2px solid #34495e; border-radius: 12px; }
        QFrame QLabel { background-color: transparent; font-size: 18px; font-weight: bold; padding: 5px; }

        QPushButton {
            background-color: #34495e;
            border: none;
            border-radius: 8px;
            color: #bdc3c7;
            padding: 10px;
            font-weight: bold;
        }
        QPushButton:pressed { background-color: #2c3e50; padding-top: 13px; padding-left: 13px; }

        /* 객체 탐지 버튼 */
        QPushButton[text="Object Detection ON"]:checked {
            background-color: #27ae60;
            color: white;
            border: 2px solid #b8e994;
            font-size: 18px;
        }
        QPushButton[text="Object Detection OFF"] { background-color: #34495e; color: #95a5a6; }

        /* 마이크 버튼 */
        QPushButton[text="MIC ON (Streaming)"]:checked {
            background-color: #2980b9;
            color: white;
            border: 2px solid #85c1e9;
            font-size: 18px;
        }
        QPushButton[text="MIC OFF"] { background-color: #34495e; color: #95a5a6; }

        /* 시스템 재부팅 버튼 */
        QPushButton[text^="SYSTEM"] { background-color: #c0392b; font-size: 20px; color: white; }
        QPushButton[text^="SYSTEM"]:hover { background-color: #e74c3c; }
    )css";

    this->setStyleSheet(style);

    // 카메라 테두리 기본값
    rgbCameraLabel->setStyleSheet("border: 3px solid #ff5252; color: #ff5252; font-weight: bold;");
    thermalCameraLabel->setStyleSheet("border: 3px solid #ffb142; color: #ffb142; font-weight: bold;");
}
