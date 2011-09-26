/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef DESKTOPCHANGEOSD_H
#define DESKTOPCHANGEOSD_H

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/WindowEffects>
#include <QWeakPointer>

class QGraphicsScene;
class QPropertyAnimation;

namespace KWin
{
class Workspace;

class DesktopChangeText : public QGraphicsItem
{
public:
    DesktopChangeText(Workspace* ws);
    ~DesktopChangeText();

    enum { Type = UserType + 2 };

    inline void setWidth(float width) {
        m_width = width;
    };
    inline void setHeight(float height) {
        m_height = height;
    };

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*);
    inline virtual int type() const {
        return Type;
    };

private:
    Workspace* m_wspace;
    float m_width;
    float m_height;
};

class DesktopChangeOSD : public QGraphicsView
{
    Q_OBJECT
public:
    DesktopChangeOSD(Workspace* ws);
    ~DesktopChangeOSD();

    inline Plasma::FrameSvg* itemFrame() {
        return &m_item_frame;
    };
    inline int& getDelayTime() {
        return m_delayTime;
    };

protected:
    virtual void hideEvent(QHideEvent*);
    virtual void drawBackground(QPainter* painter, const QRectF& rect);

private:
    void resize();
    Workspace* m_wspace;
    Plasma::FrameSvg m_frame;
    Plasma::FrameSvg m_item_frame;
    QGraphicsScene* m_scene;
    bool m_active;
    QTimer m_delayedHideTimer;
    bool m_show;
    int m_delayTime;
    bool m_textOnly;

private Q_SLOTS:
    void desktopChanged(int old);
    void numberDesktopsChanged();
    void reconfigure();
};

class DesktopChangeItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_PROPERTY(qreal arrowValue READ arrowValue WRITE setArrowValue)
    Q_PROPERTY(qreal highLightValue READ highLightValue WRITE setHighLightValue)

    Q_INTERFACES(QGraphicsItem)
public:
    DesktopChangeItem(Workspace* ws, DesktopChangeOSD* parent, int desktop);
    ~DesktopChangeItem();
    enum { Type = UserType + 1 };
    void startDesktopHighLightAnimation(int time);
    void stopDesktopHighLightAnimation();

    inline void setWidth(float width) {
        m_width = width;
    };
    inline void setHeight(float height) {
        m_height = height;
    };
    inline int desktop() const {
        return m_desktop;
    };

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*);
    inline virtual int type() const {
        return Type;
    };

    enum Arrow {
        NONE,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };
    void setArrow(Arrow arrow, int start_delay, int hide_delay);

    qreal arrowValue() const;
    qreal highLightValue() const;

protected slots:
    void setArrowValue(qreal value);
    void setHighLightValue(qreal value);

private slots:
    void showArrow();
    void hideArrow();
    void arrowAnimationFinished();

private:
    Workspace* m_wspace;
    DesktopChangeOSD* m_parent;
    int m_desktop;
    float m_width;
    float m_height;
    QTimer m_delayed_show_arrow_timer;
    QTimer m_delayed_hide_arrow_timer;

    Arrow m_arrow;
    bool m_arrowShown;
    bool m_fadeInArrow;
    bool m_fadeInHighLight;

    qreal m_arrowValue;
    qreal m_highLightValue;

    QWeakPointer<QPropertyAnimation> m_arrowAnimation;
    QWeakPointer<QPropertyAnimation> m_highLightAnimation;
};

}

#endif // DESKTOPCHANGEOSD_H
