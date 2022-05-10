/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-wp-surface-suspension-v1.h"

#include <QPointer>

namespace KWaylandServer
{

class SurfaceInterface;

class SurfaceSuspensionV1Interface : public QtWaylandServer::wp_surface_suspension_v1
{
public:
    explicit SurfaceSuspensionV1Interface(SurfaceInterface *surface);

    bool isSuspended() const;
    void setSuspended(bool suspended);

    static SurfaceSuspensionV1Interface *get(SurfaceInterface *surface);

protected:
    void wp_surface_suspension_v1_destroy(Resource *resource) override;

private:
    QPointer<SurfaceInterface> m_surface;
    bool m_suspended = false;
};

} // namespace KWaylandServer
