/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

namespace KDecoration2
{
class Decoration;
class DecorationShadow;
class DecorationSettings;

namespace Preview
{
class PreviewBridge;
class PreviewClient;
class Settings;

class PreviewItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::Decoration *decoration READ decoration NOTIFY decorationChanged)
    Q_PROPERTY(KDecoration2::Preview::PreviewBridge *bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)
    Q_PROPERTY(KDecoration2::Preview::Settings *settings READ settings WRITE setSettings NOTIFY settingsChanged)
    Q_PROPERTY(KDecoration2::Preview::PreviewClient *client READ client)
    Q_PROPERTY(KDecoration2::DecorationShadow *shadow READ shadow NOTIFY shadowChanged)
    Q_PROPERTY(QColor windowColor READ windowColor WRITE setWindowColor NOTIFY windowColorChanged)
    Q_PROPERTY(bool drawBackground READ isDrawingBackground WRITE setDrawingBackground NOTIFY drawingBackgroundChanged)
public:
    PreviewItem(QQuickItem *parent = nullptr);
    ~PreviewItem() override;
    void paint(QPainter *painter) override;

    KDecoration2::Decoration *decoration() const;
    void setDecoration(KDecoration2::Decoration *deco);

    QColor windowColor() const;
    void setWindowColor(const QColor &color);

    bool isDrawingBackground() const;
    void setDrawingBackground(bool set);

    PreviewBridge *bridge() const;
    void setBridge(PreviewBridge *bridge);

    Settings *settings() const;
    void setSettings(Settings *settings);

    PreviewClient *client();
    DecorationShadow *shadow() const;

Q_SIGNALS:
    void decorationChanged(KDecoration2::Decoration *deco);
    void windowColorChanged(const QColor &color);
    void drawingBackgroundChanged(bool);
    void bridgeChanged();
    void settingsChanged();
    void shadowChanged();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void componentComplete() override;

private:
    void paintShadow(QPainter *painter, int &paddingLeft, int &paddingRight, int &paddingTop, int &paddingBottom);
    void syncSize();
    void createDecoration();
    Decoration *m_decoration;
    QColor m_windowColor;
    bool m_drawBackground = true;
    QPointer<KDecoration2::Preview::PreviewBridge> m_bridge;
    QPointer<KDecoration2::Preview::Settings> m_settings;
    QPointer<KDecoration2::Preview::PreviewClient> m_client;
};

} // Preview
} // KDecoration2
