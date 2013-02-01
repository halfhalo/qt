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

#include "directfbwindowsurface.h"
#include "directfbglscreen.h"
#include "directfbpaintengine.h"

#include <private/qwidget_p.h>
#include <qwidget.h>
#include <qwindowsystem_qws.h>
#include <qpaintdevice.h>
#include <qvarlengtharray.h>

#include <QGraphicsView>

QT_BEGIN_NAMESPACE

DirectFBWindowSurface::DirectFBWindowSurface(DFBSurfaceFlipFlags flip, DirectFBGLScreen *scr)
    : DirectFBPaintDevice(scr)
    , dfbWindow(0)
    , flipFlags(flip)
    , boundingRectFlip(scr->directFBFlags() & DirectFBGLScreen::BoundingRectFlip)
    , flushPending(false)
{
    setSurfaceFlags(Opaque | Buffered);
#ifdef QT_DIRECTFB_TIMING
    frames = 0;
    timer.start();
#endif
}

DirectFBWindowSurface::DirectFBWindowSurface(DFBSurfaceFlipFlags flip, DirectFBGLScreen *scr, QWidget *widget)
    : QWSWindowSurface(widget), DirectFBPaintDevice(scr)
    , dfbWindow(0)
    , flipFlags(flip)
    , boundingRectFlip(scr->directFBFlags() & DirectFBGLScreen::BoundingRectFlip)
    , flushPending(false)
{
    SurfaceFlags flags = 0;
    if (!widget || widget->window()->windowOpacity() == 0xff)
        flags |= Opaque;
    setSurfaceFlags(flags);
#ifdef QT_DIRECTFB_TIMING
    frames = 0;
    timer.start();
#endif

}

DirectFBWindowSurface::~DirectFBWindowSurface()
{
    releaseSurface();
    // these are not tracked by DirectFBGLScreen so we don't want DirectFBPaintDevice to release it
    releaseWindow();
}

bool DirectFBWindowSurface::isValid() const
{
    return true;
}

void DirectFBWindowSurface::raise()
{
    if (IDirectFBWindow *window = directFBWindow()) {
        window->RaiseToTop(window);
    }
}

IDirectFBWindow *DirectFBWindowSurface::directFBWindow() const
{
    return dfbWindow;
}

void DirectFBWindowSurface::createWindow(const QRect &rect)
{
    IDirectFBDisplayLayer *layer = screen->dfbDisplayLayer();
    if (!layer)
        qFatal("DirectFBWindowSurface: Unable to get primary display layer!");

   updateIsOpaque();

    DFBWindowDescription description;
    memset(&description, 0, sizeof(DFBWindowDescription));

    description.flags = DWDESC_CAPS|DWDESC_HEIGHT|DWDESC_WIDTH|DWDESC_POSX|DWDESC_POSY|DWDESC_SURFACE_CAPS|DWDESC_PIXELFORMAT;
    description.caps = DWCAPS_NODECORATION;
    description.surface_caps = DSCAPS_NONE;
    imageFormat = screen->pixelFormat();

    if (!(surfaceFlags() & Opaque)) {
        qWarning("[QWSSurface-%p] Surface is not opaque, enabling alphachannel!", this);
        imageFormat = screen->alphaPixmapFormat();
        description.caps |= DWCAPS_ALPHACHANNEL;
#if (Q_DIRECTFB_VERSION >= 0x010200)
        description.flags |= DWDESC_OPTIONS;
        description.options |= DWOP_ALPHACHANNEL;
#endif
    }

    description.pixelformat = DirectFBGLScreen::getSurfacePixelFormat(imageFormat);
    //qWarning("Window: x:%i,y:%i,w:%i,h:%i",rect.x(),rect.y(),
    //         rect.width(), rect.height());
    description.posx = rect.x();
    description.posy = rect.y();
    description.width = rect.width();
    description.height = rect.height();

   if (DirectFBGLScreen::isPremultiplied(imageFormat))
        description.surface_caps = DSCAPS_PREMULTIPLIED;

    if (screen->directFBFlags() & DirectFBGLScreen::VideoOnly)
        description.surface_caps |= DSCAPS_VIDEOONLY;

    DFBResult result = layer->CreateWindow(layer, &description, &dfbWindow);
    //qDebug("[QWSSurface-%x]  Layer: %x",this, layer);
    //qDebug("[QWSSurface-%x]  Window created: %x",this, dfbWindow);

    if (result != DFB_OK)
        DirectFBErrorFatal("DirectFBWindowSurface::createWindow", result);

    if (window()) {
        if (window()->windowFlags() & Qt::WindowStaysOnTopHint) {
            dfbWindow->SetStackingClass(dfbWindow, DWSC_UPPER);
        }
        DFBWindowID winid;
        result = dfbWindow->GetID(dfbWindow, &winid);
        if (result != DFB_OK) {
            DirectFBError("DirectFBWindowSurface::createWindow. Can't get ID", result);
        } else {
            window()->setProperty("_q_DirectFBWindowID", winid);

        }
    }

    Q_ASSERT(!dfbSurface);
    dfbWindow->GetSurface(dfbWindow, &dfbSurface);

}

static DFBResult setWindowGeometry(IDirectFBWindow *dfbWindow, const QRect &old, const QRect &rect)
{
    DFBResult result = DFB_OK;
    const bool isMove = old.isEmpty() || rect.topLeft() != old.topLeft();
    const bool isResize = rect.size() != old.size();

#if (Q_DIRECTFB_VERSION >= 0x010000)
    if (isResize && isMove) {
        result = dfbWindow->SetBounds(dfbWindow, rect.x(), rect.y(),
                                      rect.width(), rect.height());
    } else if (isResize) {
        result = dfbWindow->Resize(dfbWindow,
                                   rect.width(), rect.height());
    } else if (isMove) {
        result = dfbWindow->MoveTo(dfbWindow, rect.x(), rect.y());
    }
#else
    if (isResize) {
        result = dfbWindow->Resize(dfbWindow,
                                   rect.width(), rect.height());
    }
    if (isMove) {
        result = dfbWindow->MoveTo(dfbWindow, rect.x(), rect.y());
    }
#endif
    return result;
}




void DirectFBWindowSurface::setGeometry(const QRect &rect)
{
    const QRect oldRect = geometry();
    if (oldRect == rect)
        return;
    //qDebug("[QWSSurface-%x] Settting geometry: %i,%i",this,rect.width(),rect.height());

    IDirectFBSurface *oldSurface = dfbSurface;
    const bool sizeChanged = oldRect.size() != rect.size();
    if (sizeChanged) {
       //qDebug("[QWSSurface-%x] Releasing surface :%x",this, dfbSurface);
        delete engine;
        engine = 0;
        releaseSurface();
        Q_ASSERT(!dfbSurface);
    }

    if (rect.isNull()) {
        //qDebug("[QWSSurface-%x] Rect is null! window: %x",this,dfbWindow);
        if (dfbWindow) {
            if (window())
                window()->setProperty("_q_DirectFBWindowID", QVariant());
            //qDebug("[QWSSurface-%x] Releasing window: %x",this, dfbWindow);

            releaseSurface();
            releaseWindow();
            Q_ASSERT(!dfbWindow);
        }
        Q_ASSERT(!dfbSurface);
#ifdef QT_DIRECTFB_SUBSURFACE
        Q_ASSERT(!subSurface);
#endif
    } else {
        if (!dfbWindow) {
            //qDebug("[QWSSurface-%x] Creating window: %x",this,dfbWindow);
            createWindow(rect);
        } else {
            //qDebug("[QWSSurface-%x] SetWindowGeometry: %x",this, dfbWindow);
            setWindowGeometry(dfbWindow, oldRect, rect);
            Q_ASSERT(!sizeChanged || !dfbSurface);
            if (sizeChanged)
                dfbWindow->GetSurface(dfbWindow, &dfbSurface);
        }

    }
    if (oldSurface != dfbSurface) {
        imageFormat = dfbSurface ? DirectFBGLScreen::getImageFormat(dfbSurface) : QImage::Format_Invalid;
    }

    if (oldRect.size() != rect.size()) {
        QWSWindowSurface::setGeometry(rect);
    } else {
        QWindowSurface::setGeometry(rect);
    }
}

QByteArray DirectFBWindowSurface::permanentState() const
{
    QByteArray state(sizeof(SurfaceFlags) + sizeof(DFBWindowID), 0);
    char *ptr = state.data();
    SurfaceFlags flags = surfaceFlags();
    memcpy(ptr, &flags, sizeof(SurfaceFlags));
    ptr += sizeof(SurfaceFlags);
    DFBWindowID did = (DFBWindowID)(-1);
    if (dfbWindow)
        dfbWindow->GetID(dfbWindow, &did);
    memcpy(ptr, &did, sizeof(DFBWindowID));
    return state;
}

void DirectFBWindowSurface::setPermanentState(const QByteArray &state)
{
    const char *ptr = state.constData();
    IDirectFBDisplayLayer *layer = screen->dfbDisplayLayer();
    SurfaceFlags flags;
    memcpy(&flags, ptr, sizeof(SurfaceFlags));

    setSurfaceFlags(flags);
    ptr += sizeof(SurfaceFlags);
    DFBWindowID id;
    memcpy(&id, ptr, sizeof(DFBWindowID));
    if (dfbSurface)
        dfbSurface->Release(dfbSurface);
    if (id != (DFBWindowID)-1) {
        IDirectFBWindow *dw;
        layer->GetWindow(layer, id, &dw);
        if (dw->GetSurface(dw, &dfbSurface) != DFB_OK)
                dfbSurface = 0;
        dw->Release(dw);
    }
    else {
        dfbSurface = 0;
    }
}

bool DirectFBWindowSurface::scroll(const QRegion &region, int dx, int dy)
{
    if (!dfbSurface || !(flipFlags & DSFLIP_BLIT) || region.rectCount() != 1)
        return false;
    if (flushPending) {
        dfbSurface->Flip(dfbSurface, 0, DSFLIP_BLIT);
    } else {
        flushPending = true;
    }
    dfbSurface->SetBlittingFlags(dfbSurface, DSBLIT_NOFX);
    const QRect r = region.boundingRect();
    const DFBRectangle rect = { r.x(), r.y(), r.width(), r.height() };
    dfbSurface->Blit(dfbSurface, dfbSurface, &rect, r.x() + dx, r.y() + dy);
    return true;
}

bool DirectFBWindowSurface::move(const QPoint &moveBy)
{
    setGeometry(geometry().translated(moveBy));
    return true;
}

void DirectFBWindowSurface::setOpaque(bool opaque)
{
    SurfaceFlags flags = surfaceFlags();
    if (opaque != (flags & Opaque)) {
        if (opaque) {
            flags |= Opaque;
        } else {
            flags &= ~Opaque;
        }
        setSurfaceFlags(flags);
    }
}


void DirectFBWindowSurface::flush(QWidget *widget, const QRegion &region,
                                   const QPoint &offset)
{
    QWidget *win = window();
    if (!win)
        return;

#if !defined(QT_NO_QWS_PROXYSCREEN) && !defined(QT_NO_GRAPHICSVIEW)
    QWExtra *extra = qt_widget_private(widget)->extraData();
    if (extra && extra->proxyWidget)
        return;
#else
    Q_UNUSED(widget);
#endif

    const quint8 windowOpacity = quint8(win->windowOpacity() * 0xff);
    const QRect windowGeometry = geometry();
    quint8 currentOpacity;
    Q_ASSERT(dfbWindow);
    dfbWindow->GetOpacity(dfbWindow, &currentOpacity);
    if (currentOpacity != windowOpacity) {
        dfbWindow->SetOpacity(dfbWindow, windowOpacity);
    }

    screen->flipSurface(dfbSurface, flipFlags, region, offset);

#ifdef QT_DIRECTFB_TIMING
    enum { Secs = 3 };
    ++frames;
    if (timer.elapsed() >= Secs * 1000) {
        qDebug("%d fps", int(double(frames) / double(Secs)));
        frames = 0;
        timer.restart();
    }
#endif
    flushPending = false;
}

void DirectFBWindowSurface::beginPaint(const QRegion &region)
{
    if (!engine) {
        engine = new DirectFBPaintEngine(this);
    }

    if (dfbSurface) {
        const QWidget *win = window();
        if (win && win->testAttribute(Qt::WA_NoSystemBackground)) {
            DirectFBGLScreen::solidFill(dfbSurface, Qt::transparent, region);
        }
    }
    flushPending = true;
}

void DirectFBWindowSurface::endPaint(const QRegion &)
{
#ifdef QT_NO_DIRECTFB_SUBSURFACE
    unlockSurface();
#endif
}

IDirectFBSurface *DirectFBWindowSurface::directFBSurface() const
{
    return dfbSurface;
}


IDirectFBSurface *DirectFBWindowSurface::surfaceForWidget(const QWidget *widget, QRect *rect) const
{
    Q_ASSERT(widget);
    if (!dfbSurface)
        return 0;
    QWidget *win = window();
    Q_ASSERT(win);
    if (rect) {
        if (win == widget) {
            *rect = widget->rect();
        } else {
            *rect = QRect(widget->mapTo(win, QPoint(0, 0)), widget->size());
        }
    }

    Q_ASSERT(win == widget || win->isAncestorOf(widget));
    return dfbSurface;
}

void DirectFBWindowSurface::releaseSurface()
{
    if (dfbSurface) {
#ifdef QT_DIRECTFB_SUBSURFACE
        releaseSubSurface();
#else
        unlockSurface();
#endif
        dfbSurface->Release(dfbSurface);
        dfbSurface = 0;
    }
}

void DirectFBWindowSurface::releaseWindow()
{
    if (dfbWindow) {
        dfbWindow->Release(dfbWindow);
        dfbWindow = 0;
    }
}

void DirectFBWindowSurface::updateIsOpaque()
{
    //qWarning("[QWSSurface-%p] UpdateIsOpaque", this);
    const QWidget *win = window();
    Q_ASSERT(win);
    if (win->testAttribute(Qt::WA_OpaquePaintEvent) || win->testAttribute(Qt::WA_PaintOnScreen)) {
        setOpaque(true);
        return;
    }

    if (qFuzzyCompare(static_cast<float>(win->windowOpacity()), 1.0f)) {
        const QPalette &pal = win->palette();

        if (win->autoFillBackground()) {
            const QBrush &autoFillBrush = pal.brush(win->backgroundRole());
            if (autoFillBrush.style() != Qt::NoBrush && autoFillBrush.isOpaque()) {
                setOpaque(true);
                return;
            }
        }


        // Test if the widget is a QGraphicsView and if it's got a QGLWidget as a viewport
        // If so, it enables alpha channel on this DFBWindow
        if(win->inherits("QGraphicsView"))
        {
            const QGraphicsView* gview = qobject_cast<const QGraphicsView*>(win);
            if(gview->viewport()->inherits("QGLWidget"))
            {
                setOpaque(false);
                return;
            }
        }

        // Test if one QGlWidget is related to the widget
        if(!win->findChildren<QGLWidget*>().empty())
        {
            setOpaque(false);
            return;
        }

        if (win->isWindow() && !win->testAttribute(Qt::WA_NoSystemBackground)) {
            const QBrush &windowBrush = win->palette().brush(QPalette::Window);
            if (windowBrush.style() != Qt::NoBrush && windowBrush.isOpaque()) {
                setOpaque(true);
                return;
            }
       }
    }
    setOpaque(false);
}


QT_END_NAMESPACE





