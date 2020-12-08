/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AURORAE_H
#define AURORAE_H

#include <KDecoration2/Decoration>
#include <QElapsedTimer>
#include <QVariant>
#include <KCModule>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QQuickItem;

class KConfigLoader;

namespace KWin
{
class Borders;
class EffectQuickView;
}

namespace Aurorae
{

class Decoration : public KDecoration2::Decoration
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::DecoratedClient* client READ clientPointer CONSTANT)
public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Decoration() override;

    void paint(QPainter *painter, const QRect &repaintRegion) override;

    Q_INVOKABLE QVariant readConfig(const QString &key, const QVariant &defaultValue = QVariant());

    KDecoration2::DecoratedClient *clientPointer() const;

public Q_SLOTS:
    void init() override;
    void installTitleItem(QQuickItem *item);

    void updateShadow();

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

    QRect m_contentRect; //the geometry of the part of the buffer that is not a shadow when buffer was created.
    QQuickItem *m_item = nullptr;
    QQmlContext *m_qmlContext = nullptr;
    KWin::Borders *m_borders;
    KWin::Borders *m_maximizedBorders;
    KWin::Borders *m_extendedBorders;
    KWin::Borders *m_padding;
    QString m_themeName;

    KWin::EffectQuickView *m_view;
    QElapsedTimer m_doubleClickTimer;
};

class ThemeFinder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap themes READ themes)
public:
    explicit ThemeFinder(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    QVariantMap themes() const {
        return m_themes;
    }

public Q_SLOTS:
    bool hasConfiguration(const QString &theme) const;

private:
    void init();
    void findAllQmlThemes();
    void findAllSvgThemes();
    QVariantMap m_themes;
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

#endif
