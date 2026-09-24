# Copyright (C) 2026 The DSpace Company Ltd.
# SPDX-License-Identifier: LicenseRef-DSpace-Commercial

set_source_files_properties(
    ColorSource.qml
    ImageSource.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(DocLife_Style
    STATIC
    URI DocLife.Style
    QML_FILES
        ColorSource.qml
        ImageSource.qml
)

