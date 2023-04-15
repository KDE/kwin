/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWaylandServer
{

class Display;
class DrmClientBufferIntegrationPrivate;

/**
 * The DrmClientBufferIntegration provides a stub implementation for the wl_drm
 * protocol.
 *
 * It provides the minimum amount of information to Xwayland so it can run. No
 * ClientBuffers are provided by the DrmClientBufferIntegration. Xwayland is
 * expected to provide us linux dmabuf client buffers instead.
 *
 * Once the wl_drm protocol is no longer mandatory in Xwayland, this stub can be
 * dropped.
 */
class KWIN_EXPORT DrmClientBufferIntegration : public QObject
{
    Q_OBJECT

public:
    explicit DrmClientBufferIntegration(Display *display);
    ~DrmClientBufferIntegration() override;

    void setDevice(const QString &node);

private:
    std::unique_ptr<DrmClientBufferIntegrationPrivate> d;
};

} // namespace KWaylandServer
