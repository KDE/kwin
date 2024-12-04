/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QList>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QUuid>
#include <memory>

struct wl_resource;

namespace KWin
{

class Output;
class OutputMode;
class Display;
class OutputDeviceV2InterfacePrivate;
class OutputDeviceModeV2Interface;
class OutputDeviceModeV2InterfacePrivate;

/** @class OutputDeviceV2Interface
 *
 * Represents an output device, the difference to Output is that this output can be disabled,
 * so not currently used to display content.
 *
 * @see OutputManagementV2Interface
 */
class KWIN_EXPORT OutputDeviceV2Interface : public QObject
{
    Q_OBJECT

public:
    explicit OutputDeviceV2Interface(Display *display, Output *handle, QObject *parent = nullptr);
    ~OutputDeviceV2Interface() override;

    void remove();

    Output *handle() const;

    static OutputDeviceV2Interface *get(wl_resource *native);

private:
    void updatePhysicalSize();
    void updateGlobalPosition();
    void updateManufacturer();
    void updateModel();
    void updateSerialNumber();
    void updateEisaId();
    void updateName();
    void updateScale();
    void updateSubPixel();
    void updateTransform();
    void updateModes();
    void updateCurrentMode();
    void updateEdid();
    void updateEnabled();
    void updateUuid();
    void updateCapabilities();
    void updateOverscan();
    void updateVrrPolicy();
    void updateRgbRange();
    void updateGeometry();
    void updateHighDynamicRange();
    void updateSdrBrightness();
    void updateWideColorGamut();
    void updateAutoRotate();
    void updateIccProfilePath();
    void updateBrightnessMetadata();
    void updateBrightnessOverrides();
    void updateSdrGamutWideness();
    void updateColorProfileSource();
    void updateBrightness();
    void updateColorPowerTradeoff();
    void updateDimming();

    void scheduleDone();

    std::unique_ptr<OutputDeviceV2InterfacePrivate> d;
};

/**
 * @class OutputDeviceModeV2Interface
 *
 * Represents an output device mode.
 *
 * @see OutputDeviceV2Interface
 */
class KWIN_EXPORT OutputDeviceModeV2Interface : public QObject
{
    Q_OBJECT

public:
    OutputDeviceModeV2Interface(std::shared_ptr<OutputMode> handle, QObject *parent = nullptr);
    ~OutputDeviceModeV2Interface() override;

    std::weak_ptr<OutputMode> handle() const;

    static OutputDeviceModeV2Interface *get(wl_resource *native);

private:
    friend class OutputDeviceModeV2InterfacePrivate;
    std::unique_ptr<OutputDeviceModeV2InterfacePrivate> d;
};

}
