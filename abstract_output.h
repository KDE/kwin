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
#ifndef KWIN_ABSTRACT_OUTPUT_H
#define KWIN_ABSTRACT_OUTPUT_H

#include <kwin_export.h>

#include <QObject>
#include <QRect>
#include <QSize>

namespace KWin
{

namespace ColorCorrect {
struct GammaRamp;
}

/**
 * Generic output representation in a Wayland session
 **/
class KWIN_EXPORT AbstractOutput : public QObject
{
    Q_OBJECT
public:
    explicit AbstractOutput(QObject *parent = nullptr);
    virtual ~AbstractOutput();

    virtual QString name() const = 0;
    virtual QRect geometry() const = 0;

    /**
     * Current refresh rate in 1/ms.
     **/
    virtual int refreshRate() const = 0;

    virtual bool isInternal() const {
        return false;
    }
    virtual qreal scale() const {
        return 1.;
    }
    virtual QSize physicalSize() const {
        return QSize();
    }
    virtual Qt::ScreenOrientation orientation() const {
        return Qt::PrimaryOrientation;
    }

    virtual int getGammaRampSize() const {
        return 0;
    }
    virtual bool setGammaRamp(const ColorCorrect::GammaRamp &gamma) {
        Q_UNUSED(gamma);
        return false;
    }
};

}

#endif
