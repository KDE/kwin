/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
