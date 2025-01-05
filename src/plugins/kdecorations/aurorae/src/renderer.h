/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QRect>
#include <QUrl>

#include <memory>

class QKeyEvent;
class QMouseEvent;

class QMouseEvent;
class QKeyEvent;

class QQmlContext;
class QQuickItem;
class QQuickWindow;

namespace Aurorae
{

class Renderer : public QObject
{
    Q_OBJECT

public:
    explicit Renderer();
    ~Renderer();

    QSize size() const;

    void setGeometry(const QRect &rect);
    QRect geometry() const;

    void setOpacity(qreal opacity);
    qreal opacity() const;

    void update();

    QQuickItem *contentItem() const;
    QQuickWindow *window() const;

    void setVisible(bool visible);
    bool isVisible() const;

    void show();
    void hide();

    void setDevicePixelRatio(qreal dpr);

    QImage bufferAsImage() const;

    void forwardMouseEvent(QEvent *mouseEvent);
    void forwardKeyEvent(QKeyEvent *keyEvent);

Q_SIGNALS:
    void repaintNeeded();
    void geometryChanged(const QRect &oldGeometry, const QRect &newGeometry);
    void renderRequested();
    void sceneChanged();

private:
    void handleRenderRequested();
    void handleSceneChanged();

    class Private;
    std::unique_ptr<Private> d;
};

} // namespace Aurorae
