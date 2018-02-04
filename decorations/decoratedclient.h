/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_DECORATED_CLIENT_H
#define KWIN_DECORATED_CLIENT_H
#include "options.h"

#include <KDecoration2/Private/DecoratedClientPrivate>

#include <QObject>

namespace KWin
{

class AbstractClient;

namespace Decoration
{

class Renderer;

class DecoratedClientImpl : public QObject, public KDecoration2::ApplicationMenuEnabledDecoratedClientPrivate
{
    Q_OBJECT
public:
    explicit DecoratedClientImpl(AbstractClient *client, KDecoration2::DecoratedClient *decoratedClient, KDecoration2::Decoration *decoration);
    virtual ~DecoratedClientImpl();
    QString caption() const override;
    WId decorationId() const override;
    int desktop() const override;
    int height() const override;
    QIcon icon() const override;
    bool isActive() const override;
    bool isCloseable() const override;
    bool isKeepAbove() const override;
    bool isKeepBelow() const override;
    bool isMaximizeable() const override;
    bool isMaximized() const override;
    bool isMaximizedHorizontally() const override;
    bool isMaximizedVertically() const override;
    bool isMinimizeable() const override;
    bool isModal() const override;
    bool isMoveable() const override;
    bool isOnAllDesktops() const override;
    bool isResizeable() const override;
    bool isShadeable() const override;
    bool isShaded() const override;
    QPalette palette() const override;
    QColor color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const override;
    bool providesContextHelp() const override;
    int width() const override;
    WId windowId() const override;

    Qt::Edges adjacentScreenEdges() const override;

    bool hasApplicationMenu() const override;
    bool isApplicationMenuActive() const override;

    void requestShowToolTip(const QString &text);
    void requestHideToolTip();
    void requestClose() override;
    void requestContextHelp() override;
    void requestToggleMaximization(Qt::MouseButtons buttons) override;
    void requestMinimize() override;
    void requestShowWindowMenu() override;
    void requestShowApplicationMenu(const QRect &rect, int actionId) override;
    void requestToggleKeepAbove() override;
    void requestToggleKeepBelow() override;
    void requestToggleOnAllDesktops() override;
    void requestToggleShade() override;

    void showApplicationMenu(int actionId);

    AbstractClient *client() {
        return m_client;
    }
    Renderer *renderer() {
        return m_renderer;
    }
    void destroyRenderer();
    KDecoration2::DecoratedClient *decoratedClient() {
        return KDecoration2::DecoratedClientPrivate::client();
    }

    void signalShadeChange();

private Q_SLOTS:
    void delayedRequestToggleMaximization(Options::WindowOperation operation);

private:
    void createRenderer();
    AbstractClient *m_client;
    QSize m_clientSize;
    Renderer *m_renderer;
    QMetaObject::Connection m_compositorToggledConnection;
};

}
}

#endif
