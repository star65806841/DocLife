# DocLife
Document full life cycle management is currently mainly applied to engineering documents.
# Structure
project/
├── CMakeLists.txt
├── main.cpp
├── modules/
│   ├── Documents/
│   │   ├── CMakeLists.txt
│   │   ├── Documents.qml
│   │   ├── views/
│   │   ├── components/
│   │   ├── viewmodels/
│   │   └── business/
│   ├── Approval/
│   │   ├── CMakeLists.txt
│   │   ├── Approval.qml
│   │   └── ...
│   └── Common/
│       ├── CMakeLists.txt
│       └── ...
├── style/                        # 样式模块
│   ├── CMakeLists.txt
│   ├── Colorsource.qml
│   ├── Fontsource.qml
│   └── Sizesource.qml
├── main/                         # 主业务模块
│   ├── CMakeLists.txt
│   └── Main.qml
├── app/
│   ├── main.qml
│   └── App.qml
├── resources/
├── tests/
└── cmake/
