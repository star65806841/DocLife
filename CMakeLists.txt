# Copyright (C) 2026 The DSpace Company Ltd.
# SPDX-License-Identifier: LicenseRef-DSpace-Commercial

cmake_minimum_required(VERSION 3.22)

project(DocLife VERSION 0.0.0001 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appDocLife
    DocLife/main.cpp
)

qt_add_qml_module(appDocLife
    URI DocLife
    QML_FILES
        DocLife/Main.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appDocLife PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appDocLife
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appDocLife
    PRIVATE Qt6::Quick
)

install(TARGETS appDocLife
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
