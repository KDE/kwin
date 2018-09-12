/****************************************************************************
 * Copyright 2015  Sebastian Kügler <sebas@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef WAYLAND_SERVER_OUTPUT_CHANGESET_H
#define WAYLAND_SERVER_OUTPUT_CHANGESET_H

#include <QObject>

#include "outputdevice_interface.h"
#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

/**
 * @brief Holds a set of changes to an OutputInterface or OutputDeviceInterface.
 *
 * This class implements a set of changes that the compositor can apply to an
 * OutputInterface after OutputConfiguration::apply has been called on the client
 * side. The changes are per-configuration.
 *
 * @see OutputConfiguration
 * @since 5.5
 **/
class KWAYLANDSERVER_EXPORT OutputChangeSet : public QObject
{
    Q_OBJECT
public:
    virtual ~OutputChangeSet();

    /** Whether the enabled() property of the outputdevice changed.
     * @returns @c true if the enabled property of the outputdevice has changed.
     */
    bool enabledChanged() const;
    /** Whether the currentModeId() property of the outputdevice changed.
     * @returns @c true if the enabled property of the outputdevice has changed.
     *    bool modeChanged() const;
     */
     /** Whether the transform() property of the outputdevice changed. */
    bool transformChanged() const;
    /** Whether the currentModeId() property of the outputdevice changed.
     * @returns @c true if the currentModeId() property of the outputdevice has changed.
     */
    bool modeChanged() const;
    /** Whether the globalPosition() property of the outputdevice changed.
     * @returns @c true if the globalPosition() property of the outputdevice has changed.
     */
    bool positionChanged() const;
    /** Whether the scale() property of the outputdevice changed.
     * @returns @c true if the scale() property of the outputdevice has changed.
     */
    bool scaleChanged() const;
    /** Whether the colorCurves() property of the outputdevice changed.
     * @returns @c true if the colorCurves() property of the outputdevice has changed.
     */
    bool colorCurvesChanged() const;

    /** The new value for enabled. */
    OutputDeviceInterface::Enablement enabled() const;
    /** The new mode id.*/
    int mode() const;
    /** The new value for transform. */
    OutputDeviceInterface::Transform transform() const;
    /** The new value for globalPosition. */
    QPoint position() const;
    /** The new value for scale.
     @deprecated see scaleF
     */
    int scale() const;
    /** The new value for scale.
     * @since 5.XX
     */
    qreal scaleF() const;
    /** The new value for colorCurves.
     * @since 5.XX
     */
    OutputDeviceInterface::ColorCurves colorCurves() const;

private:
    friend class OutputConfigurationInterface;
    explicit OutputChangeSet(OutputDeviceInterface *outputdevice, QObject *parent = nullptr);


    class Private;
    QScopedPointer<Private> d;
    Private *d_func() const;
};

}
}

#endif
