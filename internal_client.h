/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_client.h"

namespace KWin
{

class KWIN_EXPORT InternalClient : public AbstractClient
{
    Q_OBJECT

public:
    explicit InternalClient(QWindow *window);
    ~InternalClient() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    QRect bufferGeometry() const override;
    QStringList activities() const override;
    void blockActivityUpdates(bool b = true) override;
    qreal bufferScale() const override;
    QString captionNormal() const override;
    QString captionSuffix() const override;
    QPoint clientContentPos() const override;
    QSize minSize() const override;
    QSize maxSize() const override;
    QRect transparentRect() const override;
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    void killWindow() override;
    bool isPopupWindow() const override;
    QByteArray windowRole() const override;
    void closeWindow() override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool isPlaceable() const override;
    bool noBorder() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool isInternal() const override;
    bool isLockScreen() const override;
    bool isOutline() const override;
    quint32 windowId() const override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override;
    void hideClient(bool hide) override;
    void resizeWithChecks(const QSize &size, ForceGeometry_t force = NormalGeometrySet) override;
    void setFrameGeometry(const QRect &rect, ForceGeometry_t force = NormalGeometrySet) override;
    AbstractClient *findModal(bool allow_itself = false) override;
    void setOnAllActivities(bool set) override;
    bool takeFocus() override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    void destroyClient() override;

    void present(const QSharedPointer<QOpenGLFramebufferObject> fbo);
    void present(const QImage &image, const QRegion &damage);
    QWindow *internalWindow() const;

protected:
    bool acceptsFocus() const override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void doMove(int x, int y) override;
    void doResizeSync() override;
    void updateCaption() override;

private:
    void requestGeometry(const QRect &rect);
    void commitGeometry(const QRect &rect);
    void setCaption(const QString &caption);
    void markAsMapped();
    void syncGeometryToInternalWindow();
    void updateInternalWindowGeometry();

    QWindow *m_internalWindow = nullptr;
    QString m_captionNormal;
    QString m_captionSuffix;
    double m_opacity = 1.0;
    NET::WindowType m_windowType = NET::Normal;
    quint32 m_windowId = 0;
    Qt::WindowFlags m_internalWindowFlags = Qt::WindowFlags();
    bool m_userNoBorder = false;

    Q_DISABLE_COPY(InternalClient)
};

}
