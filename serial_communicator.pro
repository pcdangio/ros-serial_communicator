TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
    /opt/ros/melodic/include \
    include

SOURCES += \
    src/message.cpp

HEADERS += \
    include/message.h

DISTFILES += \
    CMakeLists.txt \
    package.xml
