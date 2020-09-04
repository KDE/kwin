/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstract_client.h"

namespace KWin
{

enum class SyncMode {
    Sync,
    Async,
};

class WaylandClient : public AbstractClient
{
    Q_OBJECT

public:
    WaylandClient(KWaylandServer::SurfaceInterface *surface);

    QRect bufferGeometry() const override;
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
    bool isLocalhost() const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    AbstractClient *findModal(bool allow_itself = false) override;
    void resizeWithChecks(const QSize &size, ForceGeometry_t force = NormalGeometrySet) override;
    void setFrameGeometry(const QRect &rect, ForceGeometry_t force = NormalGeometrySet) override;
    using AbstractClient::move;
    void move(int x, int y, ForceGeometry_t force = NormalGeometrySet) override;
    void killWindow() override;
    QByteArray windowRole() const override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override;
    void hideClient(bool hide) override;

    virtual QRect frameRectToBufferRect(const QRect &rect) const;
    QRect requestedFrameGeometry() const;
    QPoint requestedPos() const;
    QSize requestedSize() const;
    QRect requestedClientGeometry() const;
    QSize requestedClientSize() const;
    bool isHidden() const;

    void updateDepth();
    void setCaption(const QString &caption);

protected:
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    bool belongsToDesktop() const override;
    void doSetActive() override;
    void updateCaption() override;

    void setPositionSyncMode(SyncMode syncMode);
    void setSizeSyncMode(SyncMode syncMode);
    void cleanGrouping();
    void cleanTabBox();

    virtual void requestGeometry(const QRect &rect);
    virtual void updateGeometry(const QRect &rect);

private:
    void updateClientOutputs();
    void updateIcon();
    void updateResourceName();
    void internalShow();
    void internalHide();

    QString m_captionNormal;
    QString m_captionSuffix;
    double m_opacity = 1.0;
    QRect m_requestedFrameGeometry;
    QRect m_bufferGeometry;
    QRect m_requestedClientGeometry;
    SyncMode m_positionSyncMode = SyncMode::Sync;
    SyncMode m_sizeSyncMode = SyncMode::Sync;
    quint32 m_windowId;
    bool m_isHidden = false;
};

} // namespace KWin
