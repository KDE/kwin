/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once
#include <QObject>

#include "kwin_export.h"
#include "qwayland-server-xdg-toplevel-tag-v1.h"

namespace KWin
{

class Display;

class KWIN_EXPORT XdgToplevelTagManagerV1 : public QObject, public QtWaylandServer::xdg_toplevel_tag_manager_v1
{
    Q_OBJECT
public:
    explicit XdgToplevelTagManagerV1(Display *display, QObject *parent);

private:
    void xdg_toplevel_tag_manager_v1_destroy(Resource *resource) override;
    void xdg_toplevel_tag_manager_v1_set_toplevel_tag(Resource *resource, ::wl_resource *toplevel, const QString &tag) override;
    void xdg_toplevel_tag_manager_v1_set_toplevel_description(Resource *resource, ::wl_resource *toplevel, const QString &description) override;
};

}
