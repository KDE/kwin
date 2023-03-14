/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "options.h"

#include <KDecoration2/Private/DecoratedClientPrivate>

#include <QDeadlineTimer>
#include <QObject>
#include <QTimer>

namespace KWin
{

class Window;

namespace Decoration
{

class DecoratedClientImpl : public QObject, public KDecoration2::ApplicationMenuEnabledDecoratedClientPrivate
{
    Q_OBJECT
public:
    explicit DecoratedClientImpl(Window *window, KDecoration2::DecoratedClient *decoratedClient, KDecoration2::Decoration *decoration);
    ~DecoratedClientImpl() override;
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
    QSize size() const override;
    int width() const override;
    QString windowClass() const override;
    WId windowId() const override;

    Qt::Edges adjacentScreenEdges() const override;

    bool hasApplicationMenu() const override;
    bool isApplicationMenuActive() const override;

    void requestShowToolTip(const QString &text) override;
    void requestHideToolTip() override;
    void requestClose() override;
    void requestContextHelp() override;
    void requestToggleMaximization(Qt::MouseButtons buttons) override;
    void requestMinimize() override;
    void requestShowWindowMenu(const QRect &rect) override;
    void requestShowApplicationMenu(const QRect &rect, int actionId) override;
    void requestToggleKeepAbove() override;
    void requestToggleKeepBelow() override;
    void requestToggleOnAllDesktops() override;
    void requestToggleShade() override;

    void showApplicationMenu(int actionId) override;

    Window *window()
    {
        return m_window;
    }
    KDecoration2::DecoratedClient *decoratedClient()
    {
        return KDecoration2::DecoratedClientPrivate::client();
    }

    void signalShadeChange();

private Q_SLOTS:
    void delayedRequestToggleMaximization(Options::WindowOperation operation);

private:
    Window *m_window;
    QSize m_clientSize;

    QString m_toolTipText;
    QTimer m_toolTipWakeUp;
    QDeadlineTimer m_toolTipFallAsleep;
    bool m_toolTipShowing = false;
};

}
}
