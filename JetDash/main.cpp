#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ==========================================
    // 🎨 스타일 시트 적용 (여기서부터 디자인 코드)
    // ==========================================
    a.setStyleSheet(
        // 1. 전체 배경 (어두운 회색)
        "QMainWindow { background-color: #2b2b2b; }"

        // 2. 그룹박스 (센서 영역 테두리) - 둥글고 흰색 테두리
        "QGroupBox { "
        "   background-color: #383838;"
        "   border: 2px solid #5c5c5c;"
        "   border-radius: 10px;"
        "   margin-top: 20px;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 10px; }"

        // 3. 라벨 (글자들) - 기본 흰색, 큼직하게
        "QLabel { "
        "   color: #e0e0e0;"
        "   font-family: 'Segoe UI', sans-serif;"
        "}"
        );
    // ==========================================

    MainWindow w;
    w.show();
    return a.exec();
}
