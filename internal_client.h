/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Martin Flöser <mgraesslin@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
    QSize clientSize() const override;
    QSize minSize() const override;
    QSize maxSize() const override;
    void debug(QDebug &stream) const override;
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
    bool noBorder() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool isInternal() const override;
    bool isLockScreen() const override;
    bool isInputMethod() const override;
    bool isOutline() const override;
    quint32 windowId() const override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override;
    void hideClient(bool hide) override;
    using AbstractClient::resizeWithChecks;
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    using AbstractClient::setFrameGeometry;
    void setFrameGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    bool supportsWindowRules() const override;
    AbstractClient *findModal(bool allow_itself = false) override;
    void setOnAllActivities(bool set) override;
    void takeFocus() override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    void updateColorScheme() override;
    void showOnScreenEdge() override;

    void destroyClient();
    void present(const QSharedPointer<QOpenGLFramebufferObject> fbo);
    void present(const QImage &image, const QRegion &damage);
    QWindow *internalWindow() const;

protected:
    bool acceptsFocus() const override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void destroyDecoration() override;
    void doMove(int x, int y) override;
    void doResizeSync() override;
    void updateCaption() override;

private:
    void createDecoration(const QRect &rect);
    void requestGeometry(const QRect &rect);
    void commitGeometry(const QRect &rect);
    void setCaption(const QString &caption);
    void markAsMapped();
    void syncGeometryToInternalWindow();
    void updateInternalWindowGeometry();

    QWindow *m_internalWindow = nullptr;
    QSize m_clientSize = QSize(0, 0);
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
