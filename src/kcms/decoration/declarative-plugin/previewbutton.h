/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <KDecoration3/DecorationButton>
#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

namespace KDecoration3
{
class Decoration;

namespace Preview
{
class PreviewBridge;
class Settings;

class PreviewButtonItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Button)
    Q_PROPERTY(KDecoration3::Preview::PreviewBridge *bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)
    Q_PROPERTY(KDecoration3::Preview::Settings *settings READ settings WRITE setSettings NOTIFY settingsChanged)
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

    KDecoration3::DecorationButtonType type() const;
    int typeAsInt() const;
    void setType(KDecoration3::DecorationButtonType type);
    void setType(int type);

    const QColor &color() const
    {
        return m_color;
    }
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
    QPointer<KDecoration3::Preview::PreviewBridge> m_bridge;
    QPointer<KDecoration3::Preview::Settings> m_settings;
    KDecoration3::Decoration *m_decoration = nullptr;
    KDecoration3::DecorationButton *m_button = nullptr;
    KDecoration3::DecorationButtonType m_type = KDecoration3::DecorationButtonType::Custom;
};

} // Preview
} // KDecoration3
