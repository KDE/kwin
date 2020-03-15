/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KWAYLAND_SERVER_SURFACEROLE_P_H
#define KWAYLAND_SERVER_SURFACEROLE_P_H

#include <QPointer>

namespace KWayland
{
namespace Server
{

class SurfaceInterface;

class SurfaceRole
{
public:
    explicit SurfaceRole(SurfaceInterface *surface);
    virtual ~SurfaceRole();

    virtual void commit() = 0;

private:
    QPointer<SurfaceInterface> m_surface;

    Q_DISABLE_COPY(SurfaceRole)
};

}
}

#endif // KWAYLAND_SERVER_SURFACEROLE_P_H
