/*
    Library for Aurorae window decoration themes.
    Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef AURORAE_AURORAEBUTTON_H
#define AURORAE_AURORAEBUTTON_H

#include "auroraetheme.h"
#include <QtGui/QGraphicsWidget>
#include <kdecoration.h>

class QTimer;
class QPropertyAnimation;

namespace Plasma {
class FrameSvg;
}

namespace Aurorae {

class AuroraeTheme;

class AuroraeButton : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal animation READ animationProgress WRITE setAnimationProgress)

public:
    AuroraeButton(AuroraeTheme *theme, AuroraeButtonType type );
    virtual ~AuroraeButton();
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);

    AuroraeButtonType buttonType() const {
        return m_type;
    }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
    virtual int type() const {
        return m_type;
    }

    qreal animationProgress() const;
    void setAnimationProgress(qreal progress);
    void setCheckable(bool checkable) {
        m_checkable = checkable;
    }
    bool isCheckable() const {
        return m_checkable;
    }
    void setChecked(bool checked) {
        m_checked = checked;
    }
    bool isChecked() const {
        return m_checked;
    }

Q_SIGNALS:
    void clicked();

protected:
    enum ButtonState {
        Active = 0x1,
        Hover = 0x2,
        Pressed = 0x4,
        Deactivated = 0x8
    };
    Q_DECLARE_FLAGS(ButtonStates, ButtonState)
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const;
    virtual void paintButton(QPainter *painter, Plasma::FrameSvg *frame, ButtonStates states);
    virtual bool isAnimating() const;
    virtual AuroraeButtonType currentType() const;

    AuroraeTheme *theme() const {
        return m_theme;
    }

    bool isHovered() const {
        return m_hovered;
    }

    bool isPressed() const {
        return m_pressed;
    }
    void setPressed(bool pressed) {
        m_pressed = pressed;
    }

private:
    AuroraeTheme *m_theme;
    AuroraeButtonType m_type;
    bool m_pressed;
    bool m_hovered;
    qreal m_animationProgress;
    QPropertyAnimation *m_animation;
    bool m_checkable;
    bool m_checked;
};

class AuroraeMaximizeButton : public AuroraeButton
{
    Q_OBJECT
public:
    AuroraeMaximizeButton(AuroraeTheme* theme);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
    void setMaximizeMode(KDecorationDefines::MaximizeMode mode);

Q_SIGNALS:
    void clicked(Qt::MouseButtons button);

protected:
    virtual AuroraeButtonType currentType() const;

private:
    Qt::MouseButton m_pressedButton;
    KDecorationDefines::MaximizeMode m_maximizeMode;
};

class AuroraeMenuButton : public AuroraeButton
{
    Q_OBJECT
public:
    AuroraeMenuButton(AuroraeTheme *theme);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

    void setIcon(const QPixmap &icon);

Q_SIGNALS:
    void doubleClicked();

private:
    QPixmap m_icon;
    QTimer *m_timer;
};

class AuroraeSpacer : public QGraphicsWidget
{
public:
    AuroraeSpacer(AuroraeTheme *theme);
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const;

private:
    AuroraeTheme *m_theme;
};

} // namespace Aurorae

#endif // AURORAE_AURORAEBUTTON_H
