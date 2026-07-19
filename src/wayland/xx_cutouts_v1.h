/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/rect.h"
#include "kwin_export.h"
#include "scene/borderradius.h"
#include "wayland/qwayland-server-xx-cutouts-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;

class KWIN_EXPORT CutoutsManagerV1 : public QObject, private QtWaylandServer::xx_cutouts_manager_v1
{
public:
    explicit CutoutsManagerV1(Display *display, QObject *parent);

private:
    void xx_cutouts_manager_v1_destroy(Resource *resource) override;
    void xx_cutouts_manager_v1_get_cutouts(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class CutoutsV1 : private QtWaylandServer::xx_cutouts_v1
{
public:
    explicit CutoutsV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~CutoutsV1() override;

    void setCutouts(const QList<RectF> &cutouts, BorderRadius corners);

private:
    void xx_cutouts_v1_destroy_resource(Resource *resource) override;
    void xx_cutouts_v1_destroy(Resource *resource) override;
    void xx_cutouts_v1_set_unhandled(Resource *resource, wl_array *unhandled) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
