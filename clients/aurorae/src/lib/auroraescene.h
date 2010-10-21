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

#ifndef AURORAE_AURORAESCENE_H
#define AURORAE_AURORAESCENE_H
// #include "libaurorae_export.h"

#include <Plasma/Corona>
#include <kdecoration.h>

class QGraphicsLayout;
class QGraphicsLinearLayout;
class QIcon;
class QPropertyAnimation;

namespace Aurorae {
class AuroraeTheme;

class AuroraeTabData
{
public:
    AuroraeTabData();
    AuroraeTabData(const QString &caption);
    AuroraeTabData(const QString &caption, const QIcon &icon, WId wid = 0);
    QString caption() const;
    void setCaption(const QString &caption);
    QIcon icon() const;
    void setIcon(const QIcon &icon);
    WId wId() const;
    void setWId(WId wid);

private:
    QString m_caption;
    QIcon m_icon;
    WId m_wId;
};

class /*LIBAURORAE_EXPORT*/ AuroraeScene : public Plasma::Corona
{
    Q_OBJECT
    Q_PROPERTY(qreal animation READ animationProgress WRITE setAnimationProgress)

public:
    AuroraeScene(AuroraeTheme *theme, const QString &leftButtons, const QString &rightButtons, bool contextHelp, QObject* parent = 0);
    virtual ~AuroraeScene();
    void updateLayout();

    void setActive(bool active, bool animate = true);
    bool isActive() const;
    void setIcon(const QIcon &icon);
    void setAllDesktops(bool all);
    bool isAllDesktops() const;
    void setKeepAbove(bool keep);
    bool isKeepAbove() const;
    void setKeepBelow(bool keep);
    bool isKeepBelow() const;
    void setShade(bool shade);
    bool isShade() const;

    void setMaximizeMode(KDecorationDefines::MaximizeMode mode);
    KDecorationDefines::MaximizeMode maximizeMode() const;

    bool isAnimating() const;
    qreal animationProgress() const;
    void setAnimationProgress(qreal progress);

    int leftButtonsWidth() const;
    int rightButtonsWidth() const;
    void setButtons(const QString &left, const QString &right);

    /**
    * Updates the caption for the decoration with given index.
    * @param caption The new caption
    * @param index The index of the tab
    */
    void setCaption(const QString &caption, int index = 0);
    /**
    * Updates the captions of all tabs.
    * @param captions The new captions
    */
    void setCaptions(const QStringList &captions);
    /**
     * Updates the tab data for the decoration with given index.
     * @param data The new AuroraeTabData
     * @param index The index of the tab
     * @since 4.6
     */
    void setTabData(const AuroraeTabData &data, int index = 0);
    /**
     * Updates the tab data for all tabs
     * @param data The new AuroraeTabData
     * @since 4.6
     */
    void setAllTabData(const QList<AuroraeTabData> &data);
    /**
    * Adds a tab with given caption to the end of the tab list.
    */
    void addTab(const QString &caption);
    /**
     * Adds a tab with given tab data to the end of the tab list.
     * @since 4.6
     */
    void addTab(const AuroraeTabData &data);
    /**
    * Adds a tab for each of the given captions to the end of the tab list.
    */
    void addTabs(const QStringList &captions);
    /**
     * Adds a tab for each of the given AuroraeTabData to the end of the tab list.
     * @since 4.6
     */
    void addTabs(const QList<AuroraeTabData> &data);
    /**
    * Removes the last tab from the list, if there are at least two tabs.
    */
    void removeLastTab();
    /**
    * current number of tabs
    */
    int tabCount() const;
    void setFocusedTab(int index);
    int focusedTab() const;
    /**
    * @returns true if the tab at given index is the focused one.
    */
    bool isFocusedTab(int index) const;
    /**
    * Sets the unique tab id for dragging the tab specified by \p index.
    * When the tab is clicked this will enable dragging till the mouse is released.
    */
    void setUniqueTabDragId(int index, long int id);
    const QString &leftButtons() const;
    const QString &rightButtons() const;

Q_SIGNALS:
    void menuClicked();
    void menuDblClicked();
    void showContextHelp();
    void maximize(Qt::MouseButtons button);
    void minimizeWindow();
    void closeWindow();
    void toggleOnAllDesktops();
    void toggleKeepAbove();
    void toggleKeepBelow();
    void toggleShade();
    void titlePressed(Qt::MouseButton, Qt::MouseButtons);
    void titleReleased(Qt::MouseButton, Qt::MouseButtons);
    void titleDoubleClicked();
    void titleMouseMoved(Qt::MouseButton, Qt::MouseButtons);
    void wheelEvent(int delta);
    void tabMouseButtonPress(QGraphicsSceneMouseEvent*, int);
    void tabMouseButtonRelease(QGraphicsSceneMouseEvent*, int);
    void tabRemoved(int);
    void tabMoved(int index, int before);
    void tabMovedToGroup(long int uid, int before);

    void activeChanged();

protected:
    virtual void drawBackground(QPainter* painter, const QRectF& rect);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);

private Q_SLOTS:
    void resetTheme();
    void showTooltipsChanged(bool show);

private:
    void init();
    void initButtons(QGraphicsLinearLayout *layout, const QString &buttons) const;
    QString buttonsToDirection(const QString &buttons);
    AuroraeTheme *m_theme;
    QGraphicsWidget *m_leftButtons;
    QGraphicsWidget *m_rightButtons;
    QGraphicsWidget *m_title;
    bool m_active;
    qreal m_animationProgress;
    QPropertyAnimation *m_animation;
    KDecorationDefines::MaximizeMode m_maximizeMode;
    QIcon m_iconPixmap;
    bool m_allDesktops;
    bool m_shade;
    bool m_keepAbove;
    bool m_keepBelow;
    QString m_leftButtonOrder;
    QString m_rightButtonOrder;
    bool m_dblClicked;
    bool m_contextHelp;
    int m_tabCount;
    int m_focusedTab;
};

} // namespace

#endif // AURORAE_AURORAESCENE_H
