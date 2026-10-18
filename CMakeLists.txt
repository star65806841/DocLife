cmake_minimum_required(VERSION 3.17)
project(DocLife LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Svg GuiPrivate Network)

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

    src/view/centralview/myfluentwindow.h src/view/centralview/myfluentwindow.cpp
    src/view/centralview/centralview.h src/view/centralview/centralview.cpp

    src/view/fluentqt/application.h src/view/fluentqt/application.cpp


    src/view/topMenu/topmenu.h src/view/topMenu/topmenu.cpp

    src/view/leftview/leftview.h src/view/leftview/leftview.cpp
    src/view/leftview/projecttreeview.h src/view/leftview/projecttreeview.cpp
    src/view/leftview/fluenttreeitemdelegate.h src/view/leftview/fluenttreeitemdelegate.cpp

    src/view/popWindow/projectnew.h src/view/popWindow/projectnew.cpp
    src/view/popWindow/projectsetting.h src/view/popWindow/projectsetting.cpp

    src/view/popWindow/projectinfo.h src/view/popWindow/projectinfo.cpp

    src/view/popWindow/infoItem/projectitem.h src/view/popWindow/infoItem/projectitem.cpp
    src/view/popWindow/infoItem/projectinfopage.h src/view/popWindow/infoItem/projectinfopage.cpp

    src/view/popWindow/dialog/popcontent.h src/view/popWindow/dialog/popcontent.cpp
    src/view/popWindow/dialog/dialogbase.h src/view/popWindow/dialog/dialogbase.cpp
    src/view/popWindow/dialog/popupbase.h src/view/popWindow/dialog/popupbase.cpp
    src/view/popWindow/toast/apptoast.h src/view/popWindow/toast/apptoast.cpp




    excelfile.h excelfile.cpp

    excelsheet.h excelsheet.cpp
    exceltreewidget.h exceltreewidget.cpp
)

set(DOCLIFE_MODEL
    src/helper/uisignalmanager.h src/helper/uisignalmanager.cpp
    src/helper/utils.h src/helper/utils.cpp
    src/helper/singleinstanceguard.h src/helper/singleinstanceguard.cpp

    src/business/projectcontext.h src/business/projectcontext.cpp

    src/model/projecmodel1.h
    src/model/projectInfomodel.h
    src/model/projectnode.h
    src/model/custommodel/projecttreemodel.h src/model/custommodel/projecttreemodel.cpp

)

qt_add_resources(DOCLIFE_RESOURCES
    ./resources/resource.qrc
)

qt_add_executable(DocLife
    WIN32 MACOSX_BUNDLE
    ${DOCLIFE_SRC}
    ${DOCLIFE_MODEL}
    ${DOCLIFE_RESOURCES}
)

target_include_directories(DocLife
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/helper
        ${CMAKE_CURRENT_SOURCE_DIR}/src/business
)

target_link_libraries(DocLife
    PRIVATE
        Qt::Core
        Qt::Svg
        Qt::Network
        Qt::Widgets

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
