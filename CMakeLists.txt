cmake_minimum_required(VERSION 3.17)
project(DocLife LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Svg GuiPrivate)

qt_standard_project_setup()

set(QT_NO_PRIVATE_MODULE_WARNING ON)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/QXlsx/QXlsx
    ${CMAKE_CURRENT_BINARY_DIR}/QXlsx_build
)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Fluent-Qt
    ${CMAKE_CURRENT_BINARY_DIR}/fluent_build
)
# add_subdirectory(
#     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glog
#     ${CMAKE_CURRENT_BINARY_DIR}/glog_build
# )
set(DOCLIFE_SRC
    src/main.cpp

    src/view/mainwindow.h src/view/mainwindow.cpp
    src/view/centralview/centralview.h src/view/centralview/centralview.cpp

    src/view/fluentqt/application.h src/view/fluentqt/application.cpp
    src/view/topMenu/topmenu.h src/view/topMenu/topmenu.cpp

    src/view/popWindow/projectnew.h src/view/popWindow/projectnew.cpp
    src/view/popWindow/projectsetting.h src/view/popWindow/projectsetting.cpp

    src/view/popWindow/infoItem/projectitem.h src/view/popWindow/infoItem/projectitem.cpp
    src/view/popWindow/infoItem/projectinfopage.h src/view/popWindow/infoItem/projectinfopage.cpp

    excelfile.h excelfile.cpp
    mainContent.h mainContent.cpp
    excelsheet.h excelsheet.cpp
    exceltreewidget.h exceltreewidget.cpp
)

set(DOCLIFE_MODEL
    src/helper/uisignalmanager.h src/helper/uisignalmanager.cpp
    src/helper/utils.h src/helper/utils.cpp

    src/model/projecmodel.h
)

qt_add_resources(DOCLIFE_RESOURCES
    ./resources/resource.qrc
)

qt_add_executable(DocLife
    WIN32 MACOSX_BUNDLE
    ${DOCLIFE_SRC}
    ${DOCLIFE_MODEL}
    ${DOCLIFE_RESOURCES}
    src/view/popWindow/dialog/dialogbase.h src/view/popWindow/dialog/dialogbase.cpp
    src/view/popWindow/projectinfo.h src/view/popWindow/projectinfo.cpp


)

target_include_directories(DocLife
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/helper
)

target_link_libraries(DocLife
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Svg
        QXlsx::QXlsx
        FluentQt::FluentQt
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
