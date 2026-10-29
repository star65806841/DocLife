cmake_minimum_required(VERSION 3.17)

project(DocLife LANGUAGES CXX)

set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS
    Core Widgets LinguistTools Svg GuiPrivate Network Concurrent Pdf PdfWidgets)

qt_standard_project_setup()

set(CMAKE_DISABLE_FIND_PACKAGE_Qt5 TRUE)

# ===========================================================================
# 第三方依赖
# ===========================================================================

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib
    ${CMAKE_CURRENT_BINARY_DIR}/zlib_build
)

# 帮助 QuaZip 内部的 find_package(ZLIB) 定位到刚引入的 zlib
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib)
set(ZLIB_LIBRARY zlibstatic)  # zlib 静态库的 CMake target 名

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/QXlsx/QXlsx
    ${CMAKE_CURRENT_BINARY_DIR}/QXlsx_build
)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Fluent-Qt
    ${CMAKE_CURRENT_BINARY_DIR}/fluent_build
)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/quazip
    ${CMAKE_CURRENT_BINARY_DIR}/quazip_build
)
# add_subdirectory(
#     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glog
#     ${CMAKE_CURRENT_BINARY_DIR}/glog_build
# )

# ===========================================================================
# 源文件清单（按模块分组）
# ===========================================================================

# ---------------------------------------------------------------------------
# 应用入口 & 主窗口框架
# ---------------------------------------------------------------------------
set(DOCLIFE_APP
    src/main.cpp
    src/view/fluentqt/application.h                     src/view/fluentqt/application.cpp
    src/view/fluentqt/myfluentwindow.h                  src/view/fluentqt/myfluentwindow.cpp
)

# ---------------------------------------------------------------------------
# 中央内容区（预览容器 + PDF/Excel 预览 + 单元格渲染）
# ---------------------------------------------------------------------------
set(DOCLIFE_VIEW_CENTRAL
    src/view/centralview/maincontentview.h                                   src/view/centralview/maincontentview.cpp
    src/view/centralview/centralview.h                                       src/view/centralview/centralview.cpp
    src/view/centralview/centralviewitem/templatepreviewview.h               src/view/centralview/centralviewitem/templatepreviewview.cpp
    src/view/centralview/centralviewitem/templatepdfpreviewview.h            src/view/centralview/centralviewitem/templatepdfpreviewview.cpp
    src/view/centralview/centralviewitem/excelcelldelegate.h                 src/view/centralview/centralviewitem/excelcelldelegate.cpp
    src/view/centralview/centralviewitem/excelsheetmodel.h                   src/view/centralview/centralviewitem/excelsheetmodel.cpp
)

# ---------------------------------------------------------------------------
# 顶部菜单栏
# ---------------------------------------------------------------------------
set(DOCLIFE_VIEW_MENU
    src/view/topMenu/topmenu.h                                               src/view/topMenu/topmenu.cpp
)

# ---------------------------------------------------------------------------
# 左侧栏（项目树 + 模板目录树 + 搜索）
# ---------------------------------------------------------------------------
set(DOCLIFE_VIEW_LEFT
    # 项目树
    src/view/leftview/projecttreeview/projectview.h                          src/view/leftview/projecttreeview/projectview.cpp
    src/view/leftview/projecttreeview/lefttop.h                              src/view/leftview/projecttreeview/lefttop.cpp
    src/view/leftview/projecttreeview/projecttreeview.h                      src/view/leftview/projecttreeview/projecttreeview.cpp
    src/view/leftview/projecttreeview/fluenttreeitemdelegate.h               src/view/leftview/projecttreeview/fluenttreeitemdelegate.cpp
    src/view/leftview/projecttreeview/projectcontextmenubuilder.h            src/view/leftview/projecttreeview/projectcontextmenubuilder.cpp
    # 模板视图
    src/view/leftview/templateview/templateview.h                            src/view/leftview/templateview/templateview.cpp
    src/view/leftview/templateview/catalogview.h                             src/view/leftview/templateview/catalogview.cpp
    src/view/leftview/templateview/catalogsearchview.h                       src/view/leftview/templateview/catalogsearchview.cpp
)

# ---------------------------------------------------------------------------
# 弹窗 / 对话框 / Toast
# ---------------------------------------------------------------------------
set(DOCLIFE_VIEW_POPUP
    # 项目相关
    src/view/popWindow/projectnew.h                                          src/view/popWindow/projectnew.cpp
    src/view/popWindow/projectsetting.h                                      src/view/popWindow/projectsetting.cpp
    src/view/popWindow/projectinfo.h                                         src/view/popWindow/projectinfo.cpp
    src/view/popWindow/infoItem/projectitem.h                                src/view/popWindow/infoItem/projectitem.cpp
    src/view/popWindow/infoItem/projectinfopage.h                            src/view/popWindow/infoItem/projectinfopage.cpp
    # 通用对话框与 Toast
    src/view/popWindow/dialog/popcontent.h                                   src/view/popWindow/dialog/popcontent.cpp
    src/view/popWindow/dialog/dialogbase.h                                   src/view/popWindow/dialog/dialogbase.cpp
    src/view/popWindow/dialog/popupbase.h                                    src/view/popWindow/dialog/popupbase.cpp
    src/view/popWindow/toast/apptoast.h                                      src/view/popWindow/toast/apptoast.cpp
)

# ---------------------------------------------------------------------------
# 基础设施（工具函数、UI 信号总线、单实例守卫）
# ---------------------------------------------------------------------------
set(DOCLIFE_HELPER
    src/helper/uisignalmanager.h                                             src/helper/uisignalmanager.cpp
    src/helper/utils.h                                                       src/helper/utils.cpp
    src/helper/singleinstanceguard.h                                         src/helper/singleinstanceguard.cpp
)

# ---------------------------------------------------------------------------
# 业务逻辑（项目上下文、模板目录）
# ---------------------------------------------------------------------------
set(DOCLIFE_BUSINESS
    src/business/projectcontext.h                                            src/business/projectcontext.cpp
    src/business/templatecatalog.h                                           src/business/templatecatalog.cpp
)

# ---------------------------------------------------------------------------
# 数据模型（纯头文件 + 自定义模型）
# ---------------------------------------------------------------------------
set(DOCLIFE_MODEL
    src/model/projecmodel1.h
    src/model/projectInfomodel.h
    src/model/ProjectNode.h
    src/model/custommodel/projecttreemodel.h                                 src/model/custommodel/projecttreemodel.cpp
    src/model/custommodel/nodetyperegistry.h                                 src/model/custommodel/nodetyperegistry.cpp
)

# ---------------------------------------------------------------------------
# 遗留代码（暂未迁移，建议未来 git mv 到 src/legacy/excel/）
# ---------------------------------------------------------------------------
set(DOCLIFE_LEGACY
    excelfile.h                                                              excelfile.cpp
    excelsheet.h                                                             excelsheet.cpp
    exceltreewidget.h                                                        exceltreewidget.cpp
)

# ===========================================================================
# 资源文件
# ===========================================================================

qt_add_resources(DOCLIFE_RESOURCES
    ./resources/resource.qrc
)

# ===========================================================================
# 可执行目标
# ===========================================================================

qt_add_executable(DocLife
    WIN32 MACOSX_BUNDLE
    ${DOCLIFE_APP}
    ${DOCLIFE_VIEW_CENTRAL}
    ${DOCLIFE_VIEW_MENU}
    ${DOCLIFE_VIEW_LEFT}
    ${DOCLIFE_VIEW_POPUP}
    ${DOCLIFE_HELPER}
    ${DOCLIFE_BUSINESS}
    ${DOCLIFE_MODEL}
    ${DOCLIFE_LEGACY}
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
        Qt::Concurrent
        Qt::Pdf
        Qt::PdfWidgets

        QXlsx::QXlsx
        FluentQt::FluentQt
        QuaZip::QuaZip
)

# ===========================================================================
# 构建期：种子数据复制（示例模板 zip）
# ===========================================================================

add_custom_target(DocLifeSeedData ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/data/examples.zip
        $<TARGET_FILE_DIR:DocLife>/data/examples.zip
    COMMENT "Copying seed template zip"
    VERBATIM
)
add_dependencies(DocLifeSeedData DocLife)

# ===========================================================================
# 安装
# ===========================================================================

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