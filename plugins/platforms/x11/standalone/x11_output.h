/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_X11_OUTPUT_H
#define KWIN_X11_OUTPUT_H

#include "abstract_output.h"
#include <kwin_export.h>

#include <QObject>
#include <QRect>

#include <xcb/randr.h>

namespace KWin
{

/**
 * X11 output representation
 */
class KWIN_EXPORT X11Output : public AbstractOutput
{
    Q_OBJECT

public:
    explicit X11Output(QObject *parent = nullptr);
    ~X11Output() override = default;

    QString name() const override;
    void setName(QString set);

    QRect geometry() const override;
    void setGeometry(QRect set);

    int refreshRate() const override;
    void setRefreshRate(int set);

    int gammaRampSize() const override;
    bool setGammaRamp(const GammaRamp &gamma) override;

    QSize physicalSize() const override;
    void setPhysicalSize(const QSize &size);

    QSize pixelSize() const override;

private:
    void setCrtc(xcb_randr_crtc_t crtc);
    void setGammaRampSize(int size);

    xcb_randr_crtc_t m_crtc = XCB_NONE;
    QString m_name;
    QRect m_geometry;
    QSize m_physicalSize;
    int m_gammaRampSize;
    int m_refreshRate;

    friend class X11StandalonePlatform;
};

}

#endif
