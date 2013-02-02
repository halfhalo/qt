TARGET = qpalm
TEMPLATE = lib
CONFIG += plugin $$(WEBOS_CONFIG)

QTDIR_build:DESTDIR = $$QT_BUILD_TREE/plugins/platforms
DEFINES +=QEGL_EXTRA_DEBUG
SOURCES = main.cpp \
          hiddtp_qpa.cpp \
          NyxInputControl.cpp \
          nyxkeyboardhandler.cpp

HEADERS += hidd_qpa.h \
           hiddtp_qpa.h \
           InputControl.h \
           NyxInputControl.h \
           nyxkeyboardhandler.h \
           FlickGesture.h \
           ScreenEdgeFlickGesture.h

webos {
    slate* {
        include(../fb_base/fb_base.pri)
        SOURCES += ../linuxfb/qlinuxfbintegration.cpp \
                   emulatorfbintegration.cpp
        HEADERS += ../linuxfb/qlinuxfbintegration.h \
                   emulatorfbintegration.h

        LIBS_PRIVATE += -lnyx
    } else {
        QT += opengl
        SOURCES += ../eglfs/qeglfsintegration.cpp \
                   ../eglconvenience/qeglconvenience.cpp \
                   ../eglconvenience/qeglplatformcontext.cpp \
                   ../eglfs/qeglfswindow.cpp \
                   ../eglfs/qeglfswindowsurface.cpp \
                   ../eglfs/qeglfsscreen.cpp

        HEADERS += ../eglfs/qeglfsintegration.h \
                   ../eglconvenience/qeglconvenience.h \
                   ../eglconvenience/qeglplatformcontext.h \
                   ../eglfs/qeglfswindow.h \
                   ../eglfs/qeglfswindowsurface.h \
                   ../eglfs/qeglfsscreen.h
        DEFINES += TARGET_DEVICE
        LIBS_PRIVATE += -lnyx -ldl
    }
}


INCLUDEPATH += ../clipboards
SOURCES += ../clipboards/qwebosclipboard.cpp
HEADERS += ../clipboards/qwebosclipboard.h

include(../fontdatabases/genericunix/genericunix.pri)

QMAKE_CXXFLAGS += $$QT_CFLAGS_GLIB
LIBS_PRIVATE +=$$QT_LIBS_GLIB
target.path += $$[QT_INSTALL_PLUGINS]/platforms
INSTALLS += target
