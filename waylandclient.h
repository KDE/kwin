/*
 * Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "abstract_client.h"

namespace KWin
{

class WaylandClient : public AbstractClient
{
    Q_OBJECT

public:
    WaylandClient(KWaylandServer::SurfaceInterface *surface);

    QString captionNormal() const override;
    QString captionSuffix() const override;
    QStringList activities() const override;
    void setOnAllActivities(bool set) override;
    void blockActivityUpdates(bool b = true) override;
    QPoint clientContentPos() const override;
    QRect transparentRect() const override;
    quint32 windowId() const override;
    pid_t pid() const override;
    bool isLockScreen() const override;
    bool isInputMethod() const override;
    bool isLocalhost() const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    AbstractClient *findModal(bool allow_itself = false) override;
    void resizeWithChecks(const QSize &size, ForceGeometry_t force = NormalGeometrySet) override;
    void killWindow() override;
    QByteArray windowRole() const override;

    void setCaption(const QString &caption);

protected:
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    bool belongsToDesktop() const override;
    void doSetActive() override;
    void updateCaption() override;

private:
    void updateClientArea();
    void updateClientOutputs();
    void updateIcon();
    void updateResourceName();

    QString m_captionNormal;
    QString m_captionSuffix;
    double m_opacity = 1.0;
    quint32 m_windowId;
};

} // namespace KWin
