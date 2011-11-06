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

#include "directfbglwindowsurface.h"
#include "directfbglscreen.h"
#include "directfbpaintengine.h"

#include <private/qwidget_p.h>
#include <qwidget.h>
#include <qwindowsystem_qws.h>
#include <qpaintdevice.h>
#include <qvarlengtharray.h>
#include <QtGui/private/qeglcontext_p.h>


QT_BEGIN_NAMESPACE

DirectFBGLWindowSurface::DirectFBGLWindowSurface(DFBSurfaceFlipFlags flip, DirectFBGLScreen *scr):
     sibling(0)
    , dfbWindow(0)
    , dfbSurface(0)
    , screen(scr)
    , glContext(0)
    , flipFlags(flip)
    , boundingRectFlip(scr->directFBFlags() & DirectFBGLScreen::BoundingRectFlip)
    , m_glWidget(0)
{
    setSurfaceFlags(Opaque | Buffered);
#ifdef QT_DIRECTFB_TIMING
    frames = 0;
    timer.start();
#endif
    //qDebug("[QWSGLSurface-%x] Constructor client", this);
}

DirectFBGLWindowSurface::DirectFBGLWindowSurface(DFBSurfaceFlipFlags flip, DirectFBGLScreen *scr, QGLWidget *widget)
    : QWSGLWindowSurface(widget)
    , sibling(0)
    , dfbWindow(0)
    , dfbSurface(0)
    , screen(scr)
    , glContext(0)
    , flipFlags(flip)
    , boundingRectFlip(scr->directFBFlags() & DirectFBGLScreen::BoundingRectFlip)
    , m_glWidget(widget)

{
    qDebug("[QWSGLSurface-%x] Constructor server", this);
    SurfaceFlags flags = 0;

    if (!widget || widget->window()->windowOpacity() == 0xff)
        flags |= Opaque;
    setSurfaceFlags(flags);
#ifdef QT_DIRECTFB_TIMING
    frames = 0;
    timer.start();
#endif
    m_glWidget->setWindowSurface(this);
}

DirectFBGLWindowSurface::~DirectFBGLWindowSurface()
{
    releaseContext();
    releaseSurface();
    releaseWindow();
    // these are not tracked by DirectFBGLScreen so we don't want QDirectFBPaintDevice to release it
}

bool DirectFBGLWindowSurface::isValid() const
{
    return true;
}


void DirectFBGLWindowSurface::raise()
{
    if (IDirectFBWindow *window = directFBWindow()) {
        window->RaiseToTop(window);
    }
}

IDirectFBWindow *DirectFBGLWindowSurface::directFBWindow() const
{
    return dfbWindow;
}

void DirectFBGLWindowSurface::createWindow(const QRect &rect)
{
    IDirectFBDisplayLayer *layer = screen->dfbDisplayLayer();
    if (!layer)
        qFatal("DirectFBGLWindowSurface: Unable to get primary display layer!");

    updateIsOpaque();

    DFBWindowDescription description;
    memset(&description, 0, sizeof(DFBWindowDescription));
    description.flags = DWDESC_CAPS|DWDESC_HEIGHT|DWDESC_WIDTH|DWDESC_POSX|DWDESC_POSY|DWDESC_SURFACE_CAPS|DWDESC_PIXELFORMAT;
    description.caps = DWCAPS_NODECORATION;
    description.surface_caps = DSCAPS_NONE;
    imageFormat = screen->pixelFormat();

    // Enable DSCAPS_DEPTH if the QGlWIdget requires a depth buffer
    // IDirectFBGL will set EGL_DEPTH_SIZE if the surface has DEPTH capabilities
    if(m_glWidget->context()->format().depth())
        description.surface_caps = DSCAPS_DEPTH;

    if (!(surfaceFlags() & Opaque)) {
        qWarning("[QWSGLSurface-%p] Surface is not opaque, enabling alphachannel!", this);
        imageFormat = screen->alphaPixmapFormat();
        description.caps |= DWCAPS_ALPHACHANNEL;
#if (Q_DIRECTFB_VERSION >= 0x010200)
        description.flags |= DWDESC_OPTIONS;
        description.options |= DWOP_ALPHACHANNEL;
#endif
    }
    description.pixelformat = DirectFBGLScreen::getSurfacePixelFormat(imageFormat);
    //qWarning("Rect FBGL:%i,%i,%i,%i",rect.width(),rect.height(),rect.x(),rect.y());
    description.posx = rect.x();
    description.posy = rect.y();
    description.width = rect.width();
    description.height = rect.height();

    if (DirectFBGLScreen::isPremultiplied(imageFormat))
        description.surface_caps = DSCAPS_PREMULTIPLIED;

    if (screen->directFBFlags() & DirectFBGLScreen::VideoOnly)
        description.surface_caps |= DSCAPS_VIDEOONLY;

    DFBResult result = layer->CreateWindow(layer, &description, &dfbWindow);
    //qDebug("[QWSGLSurface-%x]  Window created: %x",this, dfbWindow);
    
    if (result != DFB_OK)
        DirectFBErrorFatal("QDirectFBWindowSurface::createWindow", result);

    if (window()) {

        if (window()->windowFlags() & Qt::WindowStaysOnTopHint) {
            dfbWindow->SetStackingClass(dfbWindow, DWSC_UPPER);
        }
        DFBWindowID winid;
        result = dfbWindow->GetID(dfbWindow, &winid);
        if (result != DFB_OK) {
            DirectFBError("QDirectFBWindowSurface::createWindow. Can't get ID", result);
        } else {
            window()->setProperty("_q_DirectFBWindowID", winid);
        }
    }

    Q_ASSERT(!dfbSurface);
}

void DirectFBGLWindowSurface::createSurface()
{
    DFBResult result = dfbWindow->GetSurface(dfbWindow, &dfbSurface);
    if (result != DFB_OK)
        DirectFBErrorFatal("QDirectFBWindowSurface::createSurface", result);
}

void DirectFBGLWindowSurface::createContext()
{
    DFBResult result = dfbSurface->GetGL(dfbSurface, &glContext);
    if (result != DFB_OK)
        DirectFBErrorFatal("QDirectFBWindowSurface::createContext", result);

    // Updating the QGLContext of current QGLWidget
    // Passing the correct EGLContext handled by IDirectFBGL
    // Used for sharedContext
    lockGLContext();
    // Instantiating a new QEglContext with EGLContext held by IDirectFBGL
    QEglContext* ctx = new QEglContext();
    ctx->setContext(eglGetCurrentContext());
    // Creating a new QGLContext with this QEglContext
    QGLContext* glContext = new QGLContext(QGLFormat::defaultFormat(),m_glWidget);
    glContext->setEglContext(ctx);
    m_glWidget->setContext(glContext/*,0,true*/);
    unlockGLContext();
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

void DirectFBGLWindowSurface::setGeometry(const QRect &rect)
{
    qDebug("[QWSGLSurface-%x] Setting geometry : x:%i,y:%i,width:%i,height:%i",this,
             rect.x(),rect.y(),rect.width(),rect.height());
    const QRect oldRect = geometry();
    if (oldRect == rect)
        return;

        IDirectFBSurface *oldSurface = dfbSurface;
        const bool sizeChanged = oldRect.size() != rect.size();
        if (sizeChanged) {
            releaseContext();
            releaseSurface();

        }

        if (rect.isNull()) {
            // When the window is hidden
            // We must keep the GL context
#ifdef QT_DIRECTFB_SUBSURFACE
            Q_ASSERT(!subSurface);
#endif
        } else {
            DFBResult result = DFB_OK;
            // Create a new window and define the GL Context
            if (!dfbWindow) {
                qDebug("[QWSGLSurface-%x] Creating new DFbWIndow, DFbSurface and GLContext", this);
                createWindow(rect);
                createSurface();
                createContext();
            } else {
                setWindowGeometry(dfbWindow, oldRect, rect);
                Q_ASSERT(!sizeChanged || !dfbSurface);
                if (sizeChanged)
                {
                    qDebug("[QWSGLSurface-%x] Size changed", this);
                    releaseContext();
                    releaseSurface();
                    createSurface();
                    createContext();
                }
            }
        }
        if (oldSurface != dfbSurface)
            updateFormat();


        if (oldRect.size() != rect.size()) {
            //qWarning("QWSGLWindowSurface::setGeometry(rect)");
            QWSWindowSurface::setGeometry(rect);
        } else {
            //qWarning("QWindowSurface::setGeometry(rect)");
            QWindowSurface::setGeometry(rect);
        }
        qDebug("[DirectFBGLWindowSurface] Setting geometry done.");
}

QByteArray DirectFBGLWindowSurface::permanentState() const
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

void DirectFBGLWindowSurface::setPermanentState(const QByteArray &state)
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

bool DirectFBGLWindowSurface::scroll(const QRegion &region, int dx, int dy)
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

bool DirectFBGLWindowSurface::move(const QPoint &moveBy)
{
    qDebug("[DirectFBGLWindowSurface] Move");
    if(dfbWindow)
        setGeometry(geometry().translated(moveBy));
    // Do nothing if dfbWindow is not defined. DFBSurface is blitted on moved parent's window surface
    return true;
}

void DirectFBGLWindowSurface::setOpaque(bool opaque)
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


void DirectFBGLWindowSurface::flush(QWidget *widget, const QRegion &region,
                                   const QPoint &offset)
{
    //qDebug("[DirectFBGLWindowSurface] Flushing");
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
    const QRect windowGeometry = widget->geometry();
    quint8 currentOpacity;
    Q_ASSERT(dfbWindow);
    dfbWindow->GetOpacity(dfbWindow, &currentOpacity);
    if (currentOpacity != windowOpacity) {
        //qWarning("GL:Setting opcacity %x",currentOpacity);
        dfbWindow->SetOpacity(dfbWindow, windowOpacity);
    }


    /*qDebug("[QWSGLSurface-%x] Flipping surface (region:x:%i,y:%i,w:%i,h:%i)", this,
               region.boundingRect().x(),
               region.boundingRect().y(),
               region.boundingRect().width(),
               region.boundingRect().height());*/
    if(region.boundingRect().size() != widget->geometry().size())
        screen->flipSurface(dfbSurface, flipFlags, region,offset);
    else
        dfbSurface->Flip(dfbSurface,NULL,flipFlags);


#ifdef QT_DIRECTFB_TIMING
    enum { Secs = 3 };
    ++frames;
    if (timer.elapsed() >= Secs * 1000) {
        qWarning("[DIRECTFBGL] GLWindowSurface framerate: %d fps", int(double(frames) / double(Secs)));
        frames = 0;
        timer.restart();
    }
#endif
    flushPending = false;
}

void DirectFBGLWindowSurface::beginPaint(const QRegion &region)
{
    qDebug("[QWSGLSurface-%x] BeginPaint: making GL context current");

    QWSGLWindowSurface::beginPaint(region);
    // Lock the glContext before rendering OpenGL primitives into DFBSurface
    // this operation makes current the EGLContext
    lockGLContext();
}

void DirectFBGLWindowSurface::endPaint(const QRegion &region)
{
    unlockGLContext();
    QWSGLWindowSurface::endPaint(region);
    qDebug("[QWSGLSurface-%x] EndPaint: swapping buffer");
}

IDirectFBSurface *DirectFBGLWindowSurface::directFBSurface() const
{
    //qWarning("dfbSurface");
    if (!dfbSurface && sibling && sibling->dfbSurface)
        return sibling->dfbSurface;
    return dfbSurface;
}


IDirectFBSurface *DirectFBGLWindowSurface::surfaceForWidget(const QWidget *widget, QRect *rect) const
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

void DirectFBGLWindowSurface::releaseSurface()
{
    if (dfbSurface) {
        dfbSurface->Release(dfbSurface);
        dfbSurface = 0;
    }
}

void DirectFBGLWindowSurface::updateIsOpaque()
{
    //qWarning("[QWSGLSurface-%p] UpdateIsOpaque", this);

    const QWidget *win = window();
    Q_ASSERT(win);

    // SetOpaque to false when widget background is requested to be translucent.
    // Backward widgets will be visible through transparent areas of the widget
    if (win->testAttribute(Qt::WA_TranslucentBackground)) {
        //qWarning("[QWSGLSurface:%p] WA_TranslucentBackground detected!",this);
        setOpaque(false);
        return;
    }

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

void DirectFBGLWindowSurface::releaseContext()
{
    //qDebug("releaseContext");
    if (glContext) {
        // Ensure that context is not locked
        unlockGLContext();
        // Release the context
        glContext->Release(glContext);
        glContext = 0;
    }
}

void DirectFBGLWindowSurface::releaseWindow()
{
    //qDebug("releaseWindow");
    if (dfbWindow) {
        dfbWindow->Release(dfbWindow);
        dfbWindow = 0;
    }
}

/*!
  Returns the paint device being used for this window surface.
 */
QPaintDevice *DirectFBGLWindowSurface::paintDevice()
{
    if(!m_glWidget)
        qWarning("[DIRECTFBGL] Error : DirectFBGLSurface's Widget is null");
    return m_glWidget;
}


bool DirectFBGLWindowSurface::unlockGLContext()
{
    if (glContext) {
        DFBResult result = glContext->Unlock(glContext);
        if (result != DFB_OK)
        {
            //DirectFBErrorFatal( "Unlock GLContext", result );
            return false;
        }
        return true;
    }
    qWarning("[DIRECTFBGL] Error: Impossible to make OpenGL context current. No IDirectFBGL existing.");
    return false;
}




bool DirectFBGLWindowSurface::lockGLContext()
{
    if (glContext) {
        DFBResult result = glContext->Lock(glContext);
        if (result != DFB_OK)
        {
             //DirectFBErrorFatal( "Lock GLContext", result );
             return false;
        }
        return true;
    }
    qWarning("[DIRECTFBGL] Error: Impossible to make OpenGL context current. No IDirectFBGL existing.");
    return false;
}


void DirectFBGLWindowSurface::updateFormat()
{
    imageFormat = dfbSurface ? DirectFBGLScreen::getImageFormat(dfbSurface) : QImage::Format_Invalid;
}

QT_END_NAMESPACE
