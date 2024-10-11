/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#pragma once

#include "core/brightnessdevice.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <qwayland-server-kde-external-brightness-v1.h>

namespace KWin
{

class Display;
class ExternalBrightnessDeviceV1;
class Output;

class ExternalBrightnessV1 : public QObject, private QtWaylandServer::kde_external_brightness_v1
{
    Q_OBJECT
public:
    explicit ExternalBrightnessV1(Display *display, QObject *parent);

    QList<BrightnessDevice *> devices() const;

Q_SIGNALS:
    void devicesChanged();

private:
    void kde_external_brightness_v1_destroy(Resource *resource) override;
    void kde_external_brightness_v1_create_brightness_control(Resource *resource, uint32_t id) override;

    void addDevice(ExternalBrightnessDeviceV1 *device);
    void removeDevice(ExternalBrightnessDeviceV1 *device);

    friend class ExternalBrightnessDeviceV1;
    QList<BrightnessDevice *> m_devices;
};

class ExternalBrightnessDeviceV1 : private QtWaylandServer::kde_external_brightness_device_v1, public BrightnessDevice
{
public:
    explicit ExternalBrightnessDeviceV1(ExternalBrightnessV1 *global, wl_client *client, uint32_t id, uint32_t version);
    ~ExternalBrightnessDeviceV1() override;

    void setBrightness(double brightness) override;

    std::optional<double> observedBrightness() const override;
    bool isInternal() const override;
    QByteArray edidBeginning() const override;
    int brightnessSteps() const override;

private:
    void kde_external_brightness_device_v1_destroy_resource(Resource *resource) override;
    void kde_external_brightness_device_v1_destroy(Resource *resource) override;
    void kde_external_brightness_device_v1_set_internal(Resource *resource, uint32_t internal) override;
    void kde_external_brightness_device_v1_set_edid(Resource *resource, const QString &string) override;
    void kde_external_brightness_device_v1_set_max_brightness(Resource *resource, uint32_t value) override;
    void kde_external_brightness_device_v1_set_observed_brightness(Resource *resource, uint32_t value) override;
    void kde_external_brightness_device_v1_commit(Resource *resource) override;

    QPointer<ExternalBrightnessV1> m_global;
    QByteArray m_edidBeginning;
    std::optional<uint32_t> m_observedBrightness;
    uint32_t m_maxBrightness = 1;
    bool m_internal = false;
    bool m_done = false;
};

}
