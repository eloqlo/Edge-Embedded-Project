cmake_minimum_required(VERSION 3.16)

project(JetDash VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------
# [수정 1] 여기에 Multimedia를 반드시 추가해야 합니다!
# -----------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)

qt_standard_project_setup()

qt_add_executable(appJetDash
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

# -----------------------------------------------------------
# [수정 2] 위에서 찾은 Multimedia를 여기서 연결합니다.
# -----------------------------------------------------------
target_link_libraries(appJetDash PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

# (맨 아래에 있던 잘못된 find_package는 삭제했습니다)

set_target_properties(appJetDash PROPERTIES WIN32_EXECUTABLE TRUE)
