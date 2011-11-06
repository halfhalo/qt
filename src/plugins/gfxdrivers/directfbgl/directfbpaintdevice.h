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

#ifndef DIRECTFBPAINTDEVICE_H
#define DIRECTFBPAINTDEVICE_H

#include <private/qpaintengine_raster_p.h>
#include "directfbglscreen.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

// Inherited by both window surface and pixmap
class DirectFBPaintEngine;
class DirectFBPaintDevice : public QCustomRasterPaintDevice
{
public:
    ~DirectFBPaintDevice();

    virtual IDirectFBSurface *directFBSurface() const;

    bool lockSurface(DFBSurfaceLockFlags lockFlags);
    void unlockSurface();

    // Reimplemented from QCustomRasterPaintDevice:
    void *memory() const;
    QImage::Format format() const;
    int bytesPerLine() const;
    QSize size() const;
    int metric(QPaintDevice::PaintDeviceMetric metric) const;
    DFBSurfaceLockFlags lockFlags() const { return lockFlgs; }
    QPaintEngine *paintEngine() const;
protected:
    DirectFBPaintDevice(DirectFBGLScreen *scr);
    inline int dotsPerMeterX() const
    {
        return (screen->deviceWidth() * 1000) / screen->physicalWidth();
    }
    inline int dotsPerMeterY() const
    {
        return (screen->deviceHeight() * 1000) / screen->physicalHeight();
    }

    IDirectFBSurface *dfbSurface;
#ifdef QT_DIRECTFB_SUBSURFACE
    void releaseSubSurface();
    IDirectFBSurface *subSurface;
    friend class DirectFBPaintEnginePrivate;
    bool syncPending;
#endif
    QImage lockedImage;
    DirectFBGLScreen *screen;
    int bpl;
    DFBSurfaceLockFlags lockFlgs;
    uchar *mem;
    DirectFBPaintEngine *engine;
    QImage::Format imageFormat;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif //DIRECTFBPAINTDEVICE_H
