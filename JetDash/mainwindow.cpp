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
const QString RPI_IP = "100.92.95.100";
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

    // ---------------------------------------------------------
    // 4. 오디오 설정 (Qt 6 방식)
    // ---------------------------------------------------------
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16); // 16bit Little Endian

    QAudioDevice info = QMediaDevices::defaultAudioInput();
    if (!info.isFormatSupported(format)) {
        qWarning() << "Default audio format not supported, using preferred.";
        format = info.preferredFormat();
    }
    audioInput = new QAudioSource(info, format, this);

    // ---------------------------------------------------------
    // 5. 버튼 이벤트 연결
    // ---------------------------------------------------------

    // (1) 마이크 토글 버튼
    connect(btnMicToggle, &QPushButton::toggled, this, [this](bool checked){
        if(checked) {
            btnMicToggle->setText("MIC ON (Streaming)");
            sendJsonCommand("MIC", true);

            // 녹음 시작 -> UDP 전송
            audioDevice = audioInput->start();
            connect(audioDevice, &QIODevice::readyRead, this, &MainWindow::processAudio);
            qDebug() << "Audio Streaming STARTED";
        } else {
            btnMicToggle->setText("MIC OFF");
            sendJsonCommand("MIC", false);

            // 녹음 중지
            audioInput->stop();
            if(audioDevice) audioDevice->disconnect(this);
            qDebug() << "Audio Streaming STOPPED";
        }
        // 스타일 갱신
        btnMicToggle->style()->unpolish(btnMicToggle);
        btnMicToggle->style()->polish(btnMicToggle);
    });

    // (2) 객체 탐지 토글
    connect(btnDetectToggle, &QPushButton::toggled, this, [this](bool checked){
        if(checked) {
            btnDetectToggle->setText("Object Detection ON");
            sendJsonCommand("OBJECT_DETECTION", true);
        } else {
            btnDetectToggle->setText("Object Detection OFF");
            sendJsonCommand("OBJECT_DETECTION", false);
        }
        btnDetectToggle->style()->unpolish(btnDetectToggle);
        btnDetectToggle->style()->polish(btnDetectToggle);
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

// [슬롯] 오디오 데이터 처리 (UDP 전송)
void MainWindow::processAudio()
{
    if (!audioDevice) return;
    QByteArray data = audioDevice->readAll();
    if(data.size() > 0) {
        // 라즈베리 파이 5000번 포트로 데이터 쏘기
        udpSocket->writeDatagram(data, QHostAddress(RPI_IP), PORT_AUDIO);
    }
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

// UI 레이아웃 설정
void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QGridLayout *mainLayout = new QGridLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 카메라 화면 (Placeholder)
    rgbCameraLabel = new QLabel("RGB Camera\n(No Signal)", this);
    rgbCameraLabel->setAlignment(Qt::AlignCenter);
    thermalCameraLabel = new QLabel("Thermal Camera\n(No Signal)", this);
    thermalCameraLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(rgbCameraLabel, 0, 0);
    mainLayout->addWidget(thermalCameraLabel, 0, 1);

    // 객체 탐지 버튼
    btnDetectToggle = new QPushButton("Object Detection ON", this);
    btnDetectToggle->setCheckable(true);
    btnDetectToggle->setChecked(true);
    btnDetectToggle->setFixedHeight(50);
    btnDetectToggle->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(btnDetectToggle, 1, 0);

    // 센서 데이터 박스
    sensorBox = new QFrame(this);
    QVBoxLayout *sensorLayout = new QVBoxLayout(sensorBox);

    lblCO = new QLabel("CO Level : -", this);
    lblRollover = new QLabel("Rollover : -", this);
    lblDistance = new QLabel("Distance : -", this);
    lblSystemStatus = new QLabel("System : Connecting...", this);

    sensorLayout->addWidget(lblCO);
    sensorLayout->addWidget(lblRollover);
    sensorLayout->addWidget(lblDistance);
    sensorLayout->addWidget(lblSystemStatus);
    sensorLayout->addStretch();
    mainLayout->addWidget(sensorBox, 2, 0);

    // 컨트롤 박스
    QWidget *controlContainer = new QWidget(this);
    QHBoxLayout *controlLayout = new QHBoxLayout(controlContainer);
    controlLayout->setContentsMargins(0, 0, 0, 0);

    btnReboot = new QPushButton("SYSTEM REBOOT", this);
    btnReboot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    btnReboot->setCursor(Qt::PointingHandCursor);

    QWidget *audioContainer = new QWidget(this);
    QVBoxLayout *audioLayout = new QVBoxLayout(audioContainer);
    audioLayout->setContentsMargins(0, 0, 0, 0);

    // 마이크 버튼
    btnMicToggle = new QPushButton("MIC OFF", this);
    btnMicToggle->setCheckable(true);
    btnMicToggle->setChecked(false);
    btnMicToggle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    btnMicToggle->setCursor(Qt::PointingHandCursor);

    audioLayout->addWidget(btnMicToggle);

    controlLayout->addWidget(btnReboot, 1);
    controlLayout->addWidget(audioContainer, 2);
    mainLayout->addWidget(controlContainer, 2, 1);

    // 비율 설정
    mainLayout->setRowStretch(0, 3);
    mainLayout->setRowStretch(2, 2);
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
