#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QKeyEvent>
#include <QFrame>
#include <QTimer> // 자동 재접속용

// ★ Qt 6 오디오 헤더
#include <QAudioSource>
#include <QMediaDevices>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // 키보드 이벤트 (WASD 주행)
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void readSensorData();      // TCP 데이터 수신
    void processAudio();        // 마이크 데이터 처리 (UDP 전송)
    void attemptConnection();   // 재접속 시도 로직

private:
    void setupUi();
    void applyStyles();
    void sendJsonCommand(QString target, QJsonValue value); // JSON 전송 도우미

    // --- 통신 객체 ---
    QTcpSocket *tcpSocket;      // 명령 및 센서값 (TCP)
    QUdpSocket *udpSocket;      // 음성 전송 (UDP)
    QTimer *reconnectTimer;     // 자동 재접속 타이머

    // --- 오디오 객체 (Qt 6) ---
    QAudioSource *audioInput;
    QIODevice *audioDevice;

    // --- UI 구성요소 ---
    QWidget *centralWidget;
    QLabel *rgbCameraLabel;
    QLabel *thermalCameraLabel;
    QPushButton *btnDetectToggle;

    QFrame *sensorBox;
    QLabel *lblCO;
    QLabel *lblRollover;
    QLabel *lblDistance;
    QLabel *lblSystemStatus; // 연결 상태 표시

    QPushButton *btnReboot;
    QPushButton *btnMicToggle;
};
#endif // MAINWINDOW_H
