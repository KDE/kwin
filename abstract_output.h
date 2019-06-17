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
#include <QVector>

namespace KWin
{

class KWIN_EXPORT GammaRamp
{
public:
    GammaRamp(uint32_t size);

    /**
     * Returns the size of the gamma ramp.
     **/
    uint32_t size() const;

    /**
     * Returns pointer to the first red component in the gamma ramp.
     *
     * The returned pointer can be used for altering the red component
     * in the gamma ramp.
     **/
    uint16_t *red();

    /**
     * Returns pointer to the first red component in the gamma ramp.
     **/
    const uint16_t *red() const;

    /**
     * Returns pointer to the first green component in the gamma ramp.
     *
     * The returned pointer can be used for altering the green component
     * in the gamma ramp.
     **/
    uint16_t *green();

    /**
     * Returns pointer to the first green component in the gamma ramp.
     **/
    const uint16_t *green() const;

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     *
     * The returned pointer can be used for altering the blue component
     * in the gamma ramp.
     **/
    uint16_t *blue();

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     **/
    const uint16_t *blue() const;

private:
    QVector<uint16_t> m_table;
    uint32_t m_size;
};

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

    virtual int gammaRampSize() const {
        return 0;
    }
    virtual bool setGammaRamp(const GammaRamp &gamma) {
        Q_UNUSED(gamma);
        return false;
    }
};

}

#endif
