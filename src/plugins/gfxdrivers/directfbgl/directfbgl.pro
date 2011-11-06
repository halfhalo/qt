TARGET = qdirectfbglscreen
include(../../qpluginbase.pri)
QT += opengl

#CONFIG+=debug

# Disable qDebug() message when compiled in release
DEFINES += QT_NO_DEBUG_OUTPUT
CONFIG(debug, debug|release){
    DEFINES -= QT_NO_DEBUG_OUTPUT
}

HEADERS += directfbglscreen.h \
           directfbwindowsurface.h \
           directfbglwindowsurface.h \
           directfbpaintengine.h \
           directfbpaintdevice.h \
           directfbpixmap.h \
           directfbglkeyboard.h \
           directfbmouse.h

SOURCES += directfbglscreen.cpp \
           directfbwindowsurface.cpp \
           directfbglwindowsurface.cpp \
           directfbpaintengine.cpp \
           directfbpaintdevice.cpp \
           directfbpixmap.cpp \
           directfbglkeyboard.cpp \
           directfbmouse.cpp

QTDIR_build:DESTDIR = $$QT_BUILD_TREE/plugins/gfxdrivers

target.path = $$[QT_INSTALL_PLUGINS]/gfxdrivers
INSTALLS += target

SOURCES += directfbglscreenplugin.cpp

QMAKE_CXXFLAGS += $$QT_CFLAGS_DIRECTFB
LIBS += $$QT_LIBS_DIRECTFB
DEFINES += $$QT_DEFINES_DIRECTFB QT_DIRECTFB_TIMING QT_NO_QWS_CURSOR
contains(gfx-plugins, directfbgl):DEFINES += QT_QWS_DIRECTFBGL
