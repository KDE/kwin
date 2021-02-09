/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef KDECOARTIONS_PREVIEW_BUTTON_ITEM_H
#define KDECOARTIONS_PREVIEW_BUTTON_ITEM_H

#include <QQuickPaintedItem>
#include <QColor>
#include <QPointer>
#include <KDecoration2/DecorationButton>

namespace KDecoration2
{
class Decoration;

namespace Preview
{
class PreviewBridge;
class Settings;

class PreviewButtonItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::Preview::PreviewBridge *bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)
    Q_PROPERTY(KDecoration2::Preview::Settings *settings READ settings WRITE setSettings NOTIFY settingsChanged)
    Q_PROPERTY(int type READ typeAsInt WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    explicit PreviewButtonItem(QQuickItem *parent = nullptr);
    ~PreviewButtonItem() override;
    void paint(QPainter *painter) override;

    PreviewBridge *bridge() const;
    void setBridge(PreviewBridge *bridge);

    Settings *settings() const;
    void setSettings(Settings *settings);

    KDecoration2::DecorationButtonType type() const;
    int typeAsInt() const;
    void setType(KDecoration2::DecorationButtonType type);
    void setType(int type);

    const QColor &color() const { return m_color; }
    void setColor(const QColor &color);

Q_SIGNALS:
    void bridgeChanged();
    void typeChanged();
    void settingsChanged();

protected:
    void componentComplete() override;

private:
    void createButton();
    void syncGeometry();
    QColor m_color;
    QPointer<KDecoration2::Preview::PreviewBridge> m_bridge;
    QPointer<KDecoration2::Preview::Settings> m_settings;
    KDecoration2::Decoration *m_decoration = nullptr;
    KDecoration2::DecorationButton *m_button = nullptr;
    KDecoration2::DecorationButtonType m_type = KDecoration2::DecorationButtonType::Custom;

};

} // Preview
} // KDecoration2

#endif
