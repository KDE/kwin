/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

#include "outputdevice_v2_interface.h"

namespace KWaylandServer
{

class OutputChangeSetV2Private;

/**
 * @brief Holds a set of changes to an OutputInterface or OutputDeviceInterface.
 *
 * This class implements a set of changes that the compositor can apply to an
 * OutputInterface after OutputConfiguration::apply has been called on the client
 * side. The changes are per-configuration.
 *
 * @see OutputConfiguration
 */
class KWIN_EXPORT OutputChangeSetV2 : public QObject
{
    Q_OBJECT
public:
    ~OutputChangeSetV2() override;
    /** Whether the enabled() property of the outputdevice changed.
     * @returns @c true if the enabled property of the outputdevice has changed.
     */
    bool enabledChanged() const;

    /** Whether the  transform() property of the outputdevice changed.
     * @returns @c true if the enabled property of the outputdevice has changed.
     *    bool modeChanged() const;
     */
    /** Whether the transform() property of the outputdevice changed. */
    bool transformChanged() const;

    /** Whether the size property of the outputdevice changed.
     */
    bool sizeChanged() const;

    /** Whether the refreshRate property of the outputdevice changed.
     */
    bool refreshRateChanged() const;

    /** Whether the globalPosition() property of the outputdevice changed.
     * @returns @c true if the globalPosition() property of the outputdevice has changed.
     */
    bool positionChanged() const;

    /** Whether the scale() property of the outputdevice changed.
     * @returns @c true if the scale() property of the outputdevice has changed.
     */
    bool scaleChanged() const;

    /** Whether the overscan() property of the outputdevice changed.
     * @returns @c true if the overscan() property of the outputdevice has changed
     */
    bool overscanChanged() const;

    /**
     *  Whether the vrrPolicy() property of the outputdevice changed.
     * @returns @c true if the vrrPolicy() property of the outputdevice has changed.
     */
    bool vrrPolicyChanged() const;

    /**
     *  Whether the rgbRange() property of the outputdevice changed.
     * @returns @c true if the rgbRange() property of the outputdevice has changed.
     */
    bool rgbRangeChanged() const;

    /** The new value for enabled. */
    bool enabled() const;

    /** The new size */
    QSize size() const;

    /** The new refresh rate */
    int refreshRate() const;

    /** The new value for transform. */
    OutputDeviceV2Interface::Transform transform() const;

    /** The new value for globalPosition. */
    QPoint position() const;

    /** The new value for scale.
     */
    qreal scale() const;

    /** the overscan value in % */
    uint32_t overscan() const;

    /** The new value for vrrPolicy */
    OutputDeviceV2Interface::VrrPolicy vrrPolicy() const;

    /** The new value for rgbRange */
    OutputDeviceV2Interface::RgbRange rgbRange() const;

private:
    friend class OutputConfigurationV2InterfacePrivate;
    explicit OutputChangeSetV2(OutputDeviceV2Interface *outputdevice, QObject *parent = nullptr);

    std::unique_ptr<OutputChangeSetV2Private> d;
};

}
