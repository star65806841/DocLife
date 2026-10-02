cmake_minimum_required(VERSION 3.19)
project(DocLife LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Svg)

qt_standard_project_setup()

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/QXlsx/QXlsx/
    ${CMAKE_CURRENT_BINARY_DIR}/QXlsx_build
)

set(DOCLIFE_SRC
    main.cpp
    mainwindow.cpp
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    excelfile.h excelfile.cpp
    mainContent.h mainContent.cpp
    excelsheet.h excelsheet.cpp
    utils.h utils.cpp
    uisignalmanager.h uisignalmanager.cpp
    exceltreewidget.h exceltreewidget.cpp
)

qt_add_resources(DOCLIFE_SRC
    ./resources/resource.qrc
)

qt_add_executable(DocLife
    WIN32 MACOSX_BUNDLE
    ${DOCLIFE_SRC}
)

target_link_libraries(DocLife
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Svg
        QXlsx::QXlsx
)

install(TARGETS DocLife
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET DocLife
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
