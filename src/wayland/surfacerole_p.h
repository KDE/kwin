/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KWAYLAND_SERVER_SURFACEROLE_P_H
#define KWAYLAND_SERVER_SURFACEROLE_P_H

#include <QByteArray>
#include <QPointer>

namespace KWaylandServer
{

class SurfaceInterface;

class SurfaceRole
{
public:
    SurfaceRole(SurfaceInterface *surface, const QByteArray &name);
    virtual ~SurfaceRole();

    QByteArray name() const;
    const QPointer<SurfaceInterface> &surface() const
    {
        return m_surface;
    }

    virtual void commit() = 0;

    static SurfaceRole *get(SurfaceInterface *surface);

private:
    QPointer<SurfaceInterface> m_surface;
    QByteArray m_name;

    Q_DISABLE_COPY(SurfaceRole)
};

}

#endif // KWAYLAND_SERVER_SURFACEROLE_P_H
