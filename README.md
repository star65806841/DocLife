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
├── app/
│   ├── main.qml
│   └── App.qml
├── resources/
├── tests/
└── cmake/
