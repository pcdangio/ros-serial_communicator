TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
    /opt/ros/melodic/include \
    include

SOURCES += \
    src/communicator.cpp \
    src/inbound.cpp \
    src/message.cpp \
    src/outbound.cpp

HEADERS += \
    include/serial_communicator/communicator.h \
    include/serial_communicator/message.h \
    include/serial_communicator/message_status.h \
    include/serial_communicator/utility/inbound.h \
    include/serial_communicator/utility/outbound.h

DISTFILES += \
    CMakeLists.txt \
    README.md \
    package.xml
