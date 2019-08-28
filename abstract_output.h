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

namespace KWayland
{
namespace Server
{
class OutputChangeSet;
}
}

namespace KWin
{

class KWIN_EXPORT GammaRamp
{
public:
    GammaRamp(uint32_t size);

    /**
     * Returns the size of the gamma ramp.
     */
    uint32_t size() const;

    /**
     * Returns pointer to the first red component in the gamma ramp.
     *
     * The returned pointer can be used for altering the red component
     * in the gamma ramp.
     */
    uint16_t *red();

    /**
     * Returns pointer to the first red component in the gamma ramp.
     */
    const uint16_t *red() const;

    /**
     * Returns pointer to the first green component in the gamma ramp.
     *
     * The returned pointer can be used for altering the green component
     * in the gamma ramp.
     */
    uint16_t *green();

    /**
     * Returns pointer to the first green component in the gamma ramp.
     */
    const uint16_t *green() const;

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     *
     * The returned pointer can be used for altering the blue component
     * in the gamma ramp.
     */
    uint16_t *blue();

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     */
    const uint16_t *blue() const;

private:
    QVector<uint16_t> m_table;
    uint32_t m_size;
};

/**
 * Generic output representation.
 */
class KWIN_EXPORT AbstractOutput : public QObject
{
    Q_OBJECT

public:
    explicit AbstractOutput(QObject *parent = nullptr);
    ~AbstractOutput() override;

    /**
     * Returns the human readable name of this output.
     */
    virtual QString name() const = 0;

    /**
     * Returns the identifying uuid of this output.
     *
     * Default implementation returns an empty byte array.
     */
    virtual QByteArray uuid() const;

    /**
     * Enable or disable the output.
     *
     * Default implementation does nothing
     */
    virtual void setEnabled(bool enable);

    /**
     * This sets the changes and tests them against the specific output.
     *
     * Default implementation does nothing
     */
    virtual void applyChanges(const KWayland::Server::OutputChangeSet *changeSet);

    /**
     * Returns geometry of this output in device independent pixels.
     */
    virtual QRect geometry() const = 0;

    /**
     * Returns the approximate vertical refresh rate of this output, in mHz.
     */
    virtual int refreshRate() const = 0;

    /**
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     *
     * Default implementation returns @c false.
     */
    virtual bool isInternal() const;

    /**
     * Returns the ratio between physical pixels and logical pixels.
     *
     * Default implementation returns 1.
     */
    virtual qreal scale() const;

    /**
     * Returns the physical size of this output, in millimeters.
     *
     * Default implementation returns an invalid QSize.
     */
    virtual QSize physicalSize() const;

    /**
     * Returns the orientation of this output.
     *
     * Default implementation returns Qt::PrimaryOrientation.
     */
    virtual Qt::ScreenOrientation orientation() const;

    /**
     * Returns the size of the gamma lookup table.
     *
     * Default implementation returns 0.
     */
    virtual int gammaRampSize() const;

    /**
     * Sets the gamma ramp of this output.
     *
     * Returns @c true if the gamma ramp was successfully set.
     */
    virtual bool setGammaRamp(const GammaRamp &gamma);

private:
    Q_DISABLE_COPY(AbstractOutput)
};

} // namespace KWin

#endif
