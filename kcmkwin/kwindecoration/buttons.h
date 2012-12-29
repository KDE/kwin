/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2009, Urs Wolfer <uwolfer @ kde.org>
    Copyright (c) 2004, Sandro Giessl <sandro@giessl.com>
    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef BUTTONS_H
#define BUTTONS_H

#include <QBitmap>
#include <QListWidget>
#include <QMimeData>

class KDecorationFactory;

namespace KWin
{

/**
 * This class holds the button data.
 */
class Button
{
public:
    Button();
    Button(const QString& name, const QBitmap& icon, QChar type, bool duplicate, bool supported);
    virtual ~Button();

    QString name;
    QBitmap icon;
    QChar type;
    bool duplicate;
    bool supported;
};

class ButtonDrag : public QMimeData
{
public:
    explicit ButtonDrag(Button btn);
    ~ButtonDrag() {}

    static bool canDecode(QDropEvent* e);
    static bool decode(QDropEvent* e, Button& btn);
};

/**
 * This is plugged into ButtonDropSite
 */
class ButtonDropSiteItem
{
public:
    explicit ButtonDropSiteItem(const Button& btn);
    ~ButtonDropSiteItem();

    Button button();

    QRect rect;
    int width();
    int height();

    void draw(QPainter *p, const QPalette& cg, const QRect &rect);

private:
    Button m_button;
};

/**
 * This is plugged into ButtonSource
 */
class ButtonSourceItem : public QListWidgetItem
{
public:
    ButtonSourceItem(QListWidget * parent, const Button& btn);
    virtual ~ButtonSourceItem();

    void setButton(const Button& btn);
    Button button() const;
private:
    Button m_button;
};

/**
 * Implements the button drag source list view
 */
class ButtonSource : public QListWidget
{
    Q_OBJECT

public:
    explicit ButtonSource(QWidget *parent = 0);
    virtual ~ButtonSource();

    QSize sizeHint() const;

    void hideAllButtons();
    void showAllButtons();

    void dragMoveEvent(QDragMoveEvent *e);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void mousePressEvent(QMouseEvent *e);

signals:
    void dropped();

public slots:
    void hideButton(QChar btn);
    void showButton(QChar btn);
};

typedef QList<ButtonDropSiteItem*> ButtonList;

/**
 * This class renders and handles the demo titlebar dropsite
 */
class ButtonDropSite: public QFrame
{
    Q_OBJECT

public:
    explicit ButtonDropSite(QWidget* parent = 0);
    ~ButtonDropSite();

    // Allow external classes access our buttons - ensure buttons are
    // not duplicated however.
    ButtonList buttonsLeft;
    ButtonList buttonsRight;
    void clearLeft();
    void clearRight();

    void resizeEvent(QResizeEvent* e);
    void dragEnterEvent(QDragEnterEvent* e);
    void dragMoveEvent(QDragMoveEvent* e);
    void dragLeaveEvent(QDragLeaveEvent* e);
    void dropEvent(QDropEvent* e);
    void mousePressEvent(QMouseEvent* e); ///< Starts dragging a button...
    void paintEvent(QPaintEvent* p);

signals:
    void buttonAdded(QChar btn);
    void buttonRemoved(QChar btn);
    void changed();

public slots:
    bool removeSelectedButton(); ///< This slot is called after we drop on the item listbox...
    void recalcItemGeometry(); ///< Call this whenever the item list changes... updates the items' rect property

protected:
    ButtonDropSiteItem *buttonAt(QPoint p);
    bool removeButton(ButtonDropSiteItem *item);
    int calcButtonListWidth(const ButtonList& buttons); ///< Computes the total space the buttons will take in the titlebar
    void drawButtonList(QPainter *p, const ButtonList& buttons, int offset);

    QRect leftDropArea();
    QRect rightDropArea();

private:
    /**
     * Try to find the item. If found, set its list and index and return true, else return false
     */
    bool getItemPos(ButtonDropSiteItem *item, ButtonList* &list, int &pos);

    void cleanDropVisualizer();
    QRect m_oldDropVisualizer;

    ButtonDropSiteItem *m_selected;
};

class ButtonPositionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ButtonPositionWidget(QWidget *parent = 0);
    ~ButtonPositionWidget();

    QString buttonsLeft() const;
    QString buttonsRight() const;
    void setButtonsLeft(const QString &buttons);
    void setButtonsRight(const QString &buttons);

signals:
    void changed();

private:
    void clearButtonList(const ButtonList& btns);
    Button getButton(QChar type, bool& success);

    ButtonDropSite* m_dropSite;
    ButtonSource *m_buttonSource;

    KDecorationFactory *m_factory;
    QString m_supportedButtons;
};

} // namespace KWin

#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
