/********************************************************************
Copyright (C) 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef AURORAE_H
#define AURORAE_H

#include <KDecoration2/Decoration>
#include <QElapsedTimer>
#include <QVariant>
#include <KCModule>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;

class KConfigLoader;

namespace KWin
{
class Borders;
}

namespace Aurorae
{

class Decoration : public KDecoration2::Decoration
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::DecoratedClient* client READ clientPointer CONSTANT)
public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    virtual ~Decoration();

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
    QMouseEvent translatedMouseEvent(QMouseEvent *orig);
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;
    QImage m_buffer;
    QRect m_contentRect; //the geometry of the part of the buffer that is not a shadow when buffer was created.
    QPointer<QQuickWindow> m_view;
    QQuickItem *m_item;
    KWin::Borders *m_borders;
    KWin::Borders *m_maximizedBorders;
    KWin::Borders *m_extendedBorders;
    KWin::Borders *m_padding;
    QString m_themeName;
    QQuickRenderControl *m_renderControl = nullptr;
    QScopedPointer<QTimer> m_updateTimer;
    QScopedPointer<QOpenGLContext> m_context;
    QScopedPointer<QOffscreenSurface> m_offscreenSurface;
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
