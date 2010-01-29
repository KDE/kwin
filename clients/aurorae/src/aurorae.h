/********************************************************************
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "themeconfig.h"

#include <kdecorationfactory.h>
#include <kcommondecoration.h>
#include <KDE/Plasma/FrameSvg>

class KSelectionWatcher;
class QPropertyAnimation;

namespace Aurorae
{

class AuroraeFactory :  public QObject, public KDecorationFactoryUnstable
{
public:
    ~AuroraeFactory();

    static AuroraeFactory* instance();
    bool reset(unsigned long changed);
    KDecoration *createDecoration(KDecorationBridge*);
    bool supports(Ability ability) const;
    virtual QList< BorderSize > borderSizes() const;

    Plasma::FrameSvg *frame() {
        return &m_frame;
    }
    Plasma::FrameSvg *button(const QString& b);
    bool hasButton(const QString& button) {
        return m_buttons.contains(button);
    }
    ThemeConfig &themeConfig() {
        return m_themeConfig;
    }
    bool isValid() const {
        return m_valid;
    }

private:
    AuroraeFactory();
    void init();
    void initButtonFrame(const QString& button);
    void readThemeConfig();

private:
    static AuroraeFactory *s_instance;

    // theme name
    QString m_themeName;
    ThemeConfig m_themeConfig;
    // deco
    Plasma::FrameSvg m_frame;

    // buttons
    QHash< QString, Plasma::FrameSvg* > m_buttons;
    bool m_valid;
};

class AuroraeButton : public KCommonDecorationButton
{
    Q_OBJECT
    Q_PROPERTY(qreal animation READ animationProgress WRITE setAnimationProgress)

public:
    AuroraeButton(ButtonType type, KCommonDecoration *parent);
    void reset(unsigned long changed);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    qreal animationProgress() const;
    void setAnimationProgress(qreal progress);

protected:
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    enum ButtonState {
        Active = 0x1,
        Hover = 0x2,
        Pressed = 0x4,
        Deactivated = 0x8
    };
    Q_DECLARE_FLAGS(ButtonStates, ButtonState)
    void paintButton(QPainter& painter, Plasma::FrameSvg* frame, ButtonStates states);
    bool isAnimating() const;

private:
    qreal m_animationProgress;
    bool m_pressed;
    QPropertyAnimation *m_animation;
};


class AuroraeClient : public KCommonDecorationUnstable
{
    Q_OBJECT
    Q_PROPERTY(qreal animation READ animationProgress WRITE setAnimationProgress)
public:
    AuroraeClient(KDecorationBridge *bridge, KDecorationFactory *factory);
    ~AuroraeClient();

    virtual void init();

    virtual QString visibleName() const;
    virtual QString defaultButtonsLeft() const;
    virtual QString defaultButtonsRight() const;
    virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
    virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true,
                     const KCommonDecorationButton * = 0) const;
    virtual KCommonDecorationButton *createButton(ButtonType type);
    virtual void updateWindowShape();
    virtual void activeChange();
    virtual void captionChange();
    virtual void resize(const QSize& s);

    bool isAnimating() const;
    qreal animationProgress() const;
    void setAnimationProgress(qreal progress);

protected:
    void reset(unsigned long changed);
    void paintEvent(QPaintEvent *event);

private:
    void generateTextPixmap(QPixmap& pixmap, bool active);
    qreal m_animationProgress;
    QPixmap m_activeText;
    QPixmap m_inactiveText;
    QPropertyAnimation *m_animation;

};

}

#endif
