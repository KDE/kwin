/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-fractional-scale-v1.h"

#include <QPointer>

namespace KWaylandServer
{
class SurfaceInterface;

class FractionalScaleV1Interface : protected QtWaylandServer::wp_fractional_scale_v1
{
public:
    FractionalScaleV1Interface(SurfaceInterface *surface, wl_resource *resource);
    ~FractionalScaleV1Interface() override;

    static FractionalScaleV1Interface *get(SurfaceInterface *surface);

    void setPreferredScale(qreal scale);
    QPointer<SurfaceInterface> surface;

protected:
    void wp_fractional_scale_v1_destroy(Resource *resource) override;
    void wp_fractional_scale_v1_destroy_resource(Resource *resource) override;
};

} // namespace KWaylandServer
