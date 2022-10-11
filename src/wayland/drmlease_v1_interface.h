/*
    SPDX-FileCopyrightText: 2021-2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#pragma once

#include <QHash>
#include <QObject>
#include <map>
#include <memory>

namespace KWin
{
class DrmBackend;
class DrmGpu;
}

namespace KWaylandServer
{

class DrmLeaseDeviceV1Interface;
class DrmLeaseConnectorV1Interface;
class Display;

class DrmLeaseManagerV1 : public QObject
{
    Q_OBJECT
public:
    DrmLeaseManagerV1(KWin::DrmBackend *backend, Display *display, QObject *parent = nullptr);
    ~DrmLeaseManagerV1();

private:
    void addGpu(KWin::DrmGpu *gpu);
    void removeGpu(KWin::DrmGpu *gpu);
    void handleOutputsQueried();

    KWin::DrmBackend *const m_backend;
    Display *const m_display;
    QHash<KWin::DrmGpu *, DrmLeaseDeviceV1Interface *> m_leaseDevices;
};
}
