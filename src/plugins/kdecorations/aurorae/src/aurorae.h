/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationThemeProvider>
#include <KPluginMetaData>
#include <QElapsedTimer>
#include <QVariant>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QQuickItem;

class KConfigLoader;

namespace KWin
{
class Borders;
class OffscreenQuickView;
}

namespace Aurorae
{

class Decoration : public KDecoration2::Decoration
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::DecoratedClient *client READ clientPointer CONSTANT)
    Q_PROPERTY(QQuickItem *item READ item)
public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Decoration() override;

    void paint(QPainter *painter, const QRect &repaintRegion) override;

    Q_INVOKABLE QVariant readConfig(const QString &key, const QVariant &defaultValue = QVariant());

    KDecoration2::DecoratedClient *clientPointer() const;
    QQuickItem *item() const;

public Q_SLOTS:
    void init() override;
    void installTitleItem(QQuickItem *item);

    void updateShadow();
    void updateBlur();

Q_SIGNALS:
    void configChanged();

protected:
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupBorders(QQuickItem *item);
    void updateBorders();
    void updateBuffer();
    void updateExtendedBorders();

    bool m_supportsMask{false};

    QRect m_contentRect; // the geometry of the part of the buffer that is not a shadow when buffer was created.
    std::unique_ptr<QQuickItem> m_item;
    std::unique_ptr<QQmlContext> m_qmlContext;
    KWin::Borders *m_borders;
    KWin::Borders *m_maximizedBorders;
    KWin::Borders *m_extendedBorders;
    KWin::Borders *m_padding;
    QString m_themeName;

    std::unique_ptr<QWindow> m_dummyWindow;
    std::unique_ptr<KWin::OffscreenQuickView> m_view;
};

class ThemeProvider : public KDecoration2::DecorationThemeProvider
{
    Q_OBJECT
public:
    explicit ThemeProvider(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    QList<KDecoration2::DecorationThemeMetaData> themes() const override
    {
        return m_themes;
    }

private:
    void init();
    void findAllQmlThemes();
    void findAllSvgThemes();
    bool hasConfiguration(const QString &theme);
    QList<KDecoration2::DecorationThemeMetaData> m_themes;
    const KPluginMetaData m_data;
};

class ConfigurationModule : public KCModule
{
    Q_OBJECT
public:
    ConfigurationModule(QWidget *parent, const QVariantList &args);

private:
    void init();
    void initSvg();
    void initQml();
    QString m_theme;
    KConfigLoader *m_skeleton = nullptr;
    int m_buttonSize;
};

}
