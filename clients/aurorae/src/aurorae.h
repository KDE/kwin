/********************************************************************
Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

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

#include <kdecoration.h>
#include <kdecorationfactory.h>

class QGraphicsSceneMouseEvent;
class QGraphicsView;
class QGraphicsScene;

namespace Aurorae
{
class AuroraeTheme;
class AuroraeScene;

class AuroraeFactory :  public QObject, public KDecorationFactoryUnstable
{
public:
    ~AuroraeFactory();

    static AuroraeFactory* instance();
    bool reset(unsigned long changed);
    KDecoration *createDecoration(KDecorationBridge*);
    bool supports(Ability ability) const;
    virtual QList< BorderSize > borderSizes() const;

    AuroraeTheme *theme() const {
        return m_theme;
    }

private:
    AuroraeFactory();
    void init();

private:
    static AuroraeFactory *s_instance;

    AuroraeTheme *m_theme;
};

class AuroraeClient : public KDecorationUnstable
{
    Q_OBJECT
public:
    AuroraeClient(KDecorationBridge* bridge, KDecorationFactory* factory);
    virtual void activeChange();
    virtual void borders(int& left, int& right, int& top, int& bottom) const;
    virtual void captionChange();
    virtual void desktopChange();
    virtual void iconChange();
    virtual void init();
    virtual void maximizeChange();
    virtual QSize minimumSize() const;
    virtual Position mousePosition(const QPoint& p) const;
    virtual void resize(const QSize& s);
    virtual void shadeChange();
    // optional overrides
    virtual void padding(int &left, int &right, int &top, int &bottom) const;
    virtual void reset(long unsigned int changed);

private slots:
    void menuClicked();
    void toggleShade();
    void keepAboveChanged(bool above);
    void keepBelowChanged(bool below);
    void toggleKeepAbove();
    void toggleKeepBelow();
    void titlePressed(Qt::MouseButton button, Qt::MouseButtons buttons);
    void titleReleased(Qt::MouseButton button, Qt::MouseButtons buttons);
    void titleMouseMoved(Qt::MouseButton button, Qt::MouseButtons buttons);
    void tabMouseButtonPress(QGraphicsSceneMouseEvent *e, int index);
    void tabMouseButtonRelease(QGraphicsSceneMouseEvent *e, int index);
    void tabRemoved(int index);
    void tabMoved(int index, int before);
    void tabMovedToGroup(long int uid, int before);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);

private:
    void updateWindowShape();
    void checkTabs(bool force = false);
    AuroraeScene *m_scene;
    QGraphicsView *m_view;
    bool m_clickInProgress;
};

}

#endif
