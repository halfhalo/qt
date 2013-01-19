contains(QT_CONFIG, opengl) {
    DEFINES += QT_WAYLAND_GL_SUPPORT
    QT += opengl

HEADERS += \
    $$PWD/qwaylandglintegration.h \
    $$PWD/qwaylandglwindowsurface.h

SOURCES += \
    $$PWD/qwaylandglintegration.cpp \
    $$PWD/qwaylandglwindowsurface.cpp

    QT_WAYLAND_GL_CONFIG = wayland_egl
    QT_WAYLAND_GL_INTEGRATION = wayland_egl
    CONFIG += wayland_egl

    message("Wayland GL Integration: $$QT_WAYLAND_GL_INTEGRATION")
}


wayland_egl {
    include ($$PWD/wayland_egl/wayland_egl.pri)
}

readback_egl {
    include ($$PWD/readback_egl/readback_egl.pri)
}

readback_glx {
    include ($$PWD/readback_glx/readback_glx.pri)
}

xcomposite_glx {
    include ($$PWD/xcomposite_glx/xcomposite_glx.pri)
}

xcomposite_egl {
    include ($$PWD/xcomposite_egl/xcomposite_egl.pri)
}
