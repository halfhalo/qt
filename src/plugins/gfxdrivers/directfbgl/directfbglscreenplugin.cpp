/*-------------------------------------------------------- 
* Module Name : Qt DirectFBGL Backend
* Version : 1.5
*
* Copyright © 2011 ? 2011 France Télécom
* $QT_BEGIN_LICENSE:LGPL$
* GNU Lesser General Public License Usage
* This file may be used under the terms of the GNU Lesser General Public
* License version 2.1 as published by the Free Software Foundation and
* appearing in the file LICENSE.LGPL included in the packaging of this
* file. Please review the following information to ensure the GNU Lesser
* General Public License version 2.1 requirements will be met:
* http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
*
* In addition, as a special exception, Nokia gives you certain additional
* rights. These rights are described in the Nokia Qt LGPL Exception
* version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
*
* GNU General Public License Usage
* Alternatively, this file may be used under the terms of the GNU General
* Public License version 3.0 as published by the Free Software Foundation
* and appearing in the file LICENSE.GPL included in the packaging of this
* file. Please review the following information to ensure the GNU General
* Public License version 3.0 requirements will be met:
* http://www.gnu.org/copyleft/gpl.html.
*
* Other Usage
* Alternatively, this file may be used in accordance with the terms and
* conditions contained in a signed written agreement between you and Nokia.
*
* $QT_END_LICENSE$
*
*--------------------------------------------------------
* File Name   : directfbglscreenplugin.cpp
*
* Created     : 09/01/2011
* Author(s)   : cedric.chedaleux@orange.com
*
* Description : This backend mainly relies on preexisting DirectFB backend but adds 
*               support for OpenGL hardware acceleration. QGLwidget-based widgets are
*               fully functionnal through this backend and IDirectFBGL interface.
*               It works with DirectFB versions implementing IDirectFBGL interface. 
*               Tested over X11 and Embedded Linux
*
*
*--------------------------------------------------------
*
*/

#include "directfbglscreen.h"

#include <QScreenDriverPlugin>
#include <QStringList>


class DirectFBGLScreenDriverPlugin : public QScreenDriverPlugin
{
public:
    DirectFBGLScreenDriverPlugin();

    QStringList keys() const;
    QScreen *create(const QString&, int displayId);
};

DirectFBGLScreenDriverPlugin::DirectFBGLScreenDriverPlugin()
    : QScreenDriverPlugin()
{
}

QStringList DirectFBGLScreenDriverPlugin::keys() const
{
    return (QStringList() << "directfbgl");
}

QScreen* DirectFBGLScreenDriverPlugin::create(const QString& driver,
                                            int displayId)
{
    if (driver.toLower() != "directfbgl")
        return 0;

    printf("********** DIRECTFBGL SCREEN PLUGIN FOR QT EMBEDDED LOADED - v1.5 ***********\n");

    return new DirectFBGLScreen(displayId);
}

Q_EXPORT_PLUGIN2(qdirectfbglscreen, DirectFBGLScreenDriverPlugin)

