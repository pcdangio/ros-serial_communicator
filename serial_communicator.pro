TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
    /opt/ros/melodic/include \
    include

SOURCES += \
    src/inbound.cpp \
    src/message.cpp

HEADERS += \
    include/serial_communicator/message.h \
    include/serial_communicator/message_status.h \
    include/serial_communicator/utility/inbound.h

DISTFILES += \
    CMakeLists.txt \
    package.xml
