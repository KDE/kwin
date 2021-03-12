/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

#include "outputdevice_interface.h"
#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class OutputChangeSetPrivate;

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
    ~OutputChangeSet() override;
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
     * @since 5.50
     */
    qreal scaleF() const;
    
    /** The new value for colorCurves.
     * @since 5.XX
     */
    OutputDeviceInterface::ColorCurves colorCurves() const;

private:
    friend class OutputConfigurationInterfacePrivate;
    explicit OutputChangeSet(OutputDeviceInterface *outputdevice, QObject *parent = nullptr);

    QScopedPointer<OutputChangeSetPrivate> d;
};

}
