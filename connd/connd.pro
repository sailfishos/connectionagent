QT = core network dbus

TARGET = connectionagent
PKGCONFIG += connman-qt5

packagesExist(qt5-boostable) {
    DEFINES += HAS_BOOSTER
    PKGCONFIG += qt5-boostable
} else {
    warning("qt5-boostable not available; startup times will be slower")
}

CONFIG   += console link_pkgconfig
CONFIG   -= app_bundle

TEMPLATE = app

OTHER_FILES += com.jolla.Connectiond.xml privileges

DBUS_ADAPTORS = connadaptor
connadaptor.files = com.jollamobile.Connectiond.xml
connadaptor.header_flags = -c ConnAdaptor
connadaptor.source_flags = -c ConnAdaptor

SOURCES += main.cpp \
    qconnectionagent.cpp

HEADERS += \
    qconnectionagent.h

target.path = /usr/bin
INSTALLS += target

MOC_DIR = .moc
OBJECTS_DIR = .obj
