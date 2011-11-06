/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "directfbmouse.h"


#include "directfbglscreen.h"
#include <qsocketnotifier.h>

#include <directfb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

QT_BEGIN_NAMESPACE

class DirectFBMouseHandlerPrivate : public QObject
{
    Q_OBJECT
public:
    DirectFBMouseHandlerPrivate(DirectFBMouseHandler *h);
    ~DirectFBMouseHandlerPrivate();

    void setEnabled(bool on);
private:
    DirectFBMouseHandler *handler;
    IDirectFBEventBuffer *eventBuffer;
#ifndef QT_NO_DIRECTFB_LAYER
    IDirectFBDisplayLayer *layer;
#endif
    QSocketNotifier *mouseNotifier;

    QPoint prevPoint;
    Qt::MouseButtons prevbuttons;

    DFBEvent event;
    uint bytesRead;

private Q_SLOTS:
    void readMouseData();
};

DirectFBMouseHandlerPrivate::DirectFBMouseHandlerPrivate(DirectFBMouseHandler *h)
    : handler(h), eventBuffer(0)
{
    DFBResult result;



    QScreen *screen = QScreen::instance();
    if (!screen) {
        qCritical("DirectFBMouseHandler: no screen instance found");
        return;
    }

    IDirectFB *fb = DirectFBGLScreen::instance()->dfb();
    if (!fb) {
        qCritical("DirectFBMouseHandler: DirectFB not initialized");
        return;
    }


#ifndef QT_NO_DIRECTFB_LAYER
    layer = DirectFBGLScreen::instance()->dfbDisplayLayer();
    if (!layer) {
        qCritical("DirectFBMouseHandler: Unable to get primary display layer");
        return;
    }
#endif

    DFBInputDeviceCapabilities caps;
    caps = DICAPS_BUTTONS | DICAPS_AXES;
    result = fb->CreateInputEventBuffer(fb, caps, DFB_FALSE, &eventBuffer);
    if (result != DFB_OK) {
        DirectFBError("DirectFBMouseHandler: "
                      "Unable to create input event buffer", result);
        return;
    }

    int fd;
    result = eventBuffer->CreateFileDescriptor(eventBuffer, &fd);
    if (result != DFB_OK) {
        DirectFBError("DirectFBMouseHandler: "
                      "Unable to create file descriptor", result);
        return;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    // DirectFB seems to assume that the mouse always starts centered
    prevPoint = QPoint(screen->deviceWidth() / 2, screen->deviceHeight() / 2);
    prevbuttons = Qt::NoButton;
    memset(&event, 0, sizeof(event));
    bytesRead = 0;

    mouseNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(mouseNotifier, SIGNAL(activated(int)),this, SLOT(readMouseData()));
    setEnabled(true);
}

DirectFBMouseHandlerPrivate::~DirectFBMouseHandlerPrivate()
{
    if (eventBuffer)
        eventBuffer->Release(eventBuffer);
}

void DirectFBMouseHandlerPrivate::setEnabled(bool on)
{
    if (mouseNotifier->isEnabled() != on) {
#ifndef QT_NO_DIRECTFB_LAYER
        DFBResult result;
        result = layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE);
        if (result != DFB_OK) {
            DirectFBError("QDirectFBGLScreenCursor::QDirectFBGLScreenCursor: "
                          "Unable to set cooperative level", result);
        }
        result = layer->EnableCursor(layer, on ? 1 : 0);
        if (result != DFB_OK) {
            DirectFBError("QDirectFBGLScreenCursor::QDirectFBGLScreenCursor: "
                          "Unable to enable cursor", result);
        }

        result = layer->SetCooperativeLevel(layer, DLSCL_SHARED);
        if (result != DFB_OK) {
            DirectFBError("QDirectFBGLScreenCursor::show: "
                          "Unable to set cooperative level", result);
        }

        layer->SetCooperativeLevel(layer, DLSCL_SHARED);
#endif
        mouseNotifier->setEnabled(on);
    }

}

void DirectFBMouseHandlerPrivate::readMouseData()
{
    if (!QScreen::instance())
        return;

    for (;;) {
        // GetEvent returns DFB_UNSUPPORTED after CreateFileDescriptor().
        // This seems stupid and I really hope it's a bug which will be fixed.

        // DFBResult ret = eventBuffer->GetEvent(eventBuffer, &event);

        char *buf = reinterpret_cast<char*>(&event);
        int ret = ::read(mouseNotifier->socket(),
                         buf + bytesRead, sizeof(DFBEvent) - bytesRead);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                return;
            qWarning("DirectFBMouseHandlerPrivate::readMouseData(): %s",
                     strerror(errno));
            return;
        }

        Q_ASSERT(ret >= 0);
        bytesRead += ret;
        if (bytesRead < sizeof(DFBEvent))
            break;
        bytesRead = 0;

        Q_ASSERT(event.clazz == DFEC_INPUT);

        const DFBInputEvent input = event.input;
        int x = prevPoint.x();
        int y = prevPoint.y();
        int wheel = 0;

        if (input.type == DIET_AXISMOTION) {
#if defined(QT_NO_DIRECTFB_LAYER) || defined(QT_DIRECTFB_WINDOW_AS_CURSOR)
            if (input.flags & DIEF_AXISABS) {
                switch (input.axis) {
                case DIAI_X: x = input.axisabs; break;
                case DIAI_Y: y = input.axisabs; break;
                default:
                    qWarning("DirectFBMouseHandlerPrivate::readMouseData: "
                             "unknown axis (absolute) %d", input.axis);
                    break;
                }
            } else if (input.flags & DIEF_AXISREL) {
                switch (input.axis) {
                case DIAI_X: x += input.axisrel; break;
                case DIAI_Y: y += input.axisrel; break;
                case DIAI_Z: wheel = -120 * input.axisrel; break;
                default:
                    qWarning("DirectFBMouseHandlerPrivate::readMouseData: "
                             "unknown axis (releative) %d", input.axis);
                }
            }
#else
            if (input.axis == DIAI_X || input.axis == DIAI_Y) {
                DFBResult result = layer->GetCursorPosition(layer, &x, &y);
                if (result != DFB_OK) {
                    DirectFBError("DirectFBMouseHandler::readMouseData",
                                  result);
                }
            } else if (input.axis == DIAI_Z) {
                Q_ASSERT(input.flags & DIEF_AXISREL);
                wheel = input.axisrel;
                wheel *= -120;
            }
#endif
        }

        Qt::MouseButtons buttons = Qt::NoButton;
        if (input.flags & DIEF_BUTTONS) {
            if (input.buttons & DIBM_LEFT)
                buttons |= Qt::LeftButton;
            if (input.buttons & DIBM_MIDDLE)
                buttons |= Qt::MidButton;
            if (input.buttons & DIBM_RIGHT)
                buttons |= Qt::RightButton;
        }

        QPoint p = QPoint(x, y);
        handler->limitToScreen(p);

        if (p == prevPoint && wheel == 0 && buttons == prevbuttons)
            continue;

        prevPoint = p;
        prevbuttons = buttons;

        handler->mouseChanged(p, buttons, wheel);
    }
}

DirectFBMouseHandler::DirectFBMouseHandler(const QString &driver,
                                             const QString &device)
    : QWSMouseHandler(driver, device)
{
    d = new DirectFBMouseHandlerPrivate(this);
}

DirectFBMouseHandler::~DirectFBMouseHandler()
{
    delete d;
}

void DirectFBMouseHandler::suspend()
{
    d->setEnabled(false);
}

void DirectFBMouseHandler::resume()
{
   d->setEnabled(true);
}

QT_END_NAMESPACE
#include "directfbmouse.moc"
