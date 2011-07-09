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

#ifndef TABBOXVIEW_H
#define TABBOXVIEW_H

#include <QTableView>
/**
* @file
* This file desfines the classes which build up the view of the TabBox.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/

class QPropertyAnimation;
class QModelIndex;
namespace Plasma
{
class FrameSvg;
}

namespace KWin
{
namespace TabBox
{
class DesktopItemDelegate;
class DesktopModel;

class ClientModel;
class ClientItemDelegate;

class TabBoxMainView;
class TabBoxAdditionalView;

/**
* This class is the main view of the TabBox. It is made up of two widgets:
* TabBoxMainView which is the "switching area" for TabBox items and
* TabBoxInfoView which is an optional second list view only displaying the
* current selected item.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QRect selectedItem READ selectedItem WRITE setSelectedItem)
public:
    TabBoxView(ClientModel *clientModel, DesktopModel *desktopModel, QWidget* parent = 0);
    ~TabBoxView();
    virtual void paintEvent(QPaintEvent* e);
    virtual bool event(QEvent* event);
    virtual void resizeEvent(QResizeEvent* event);
    virtual QSize sizeHint() const;
    void updateGeometry();

    /**
    * Returns the index at the given position of the main view widget.
    * @param pos The widget position
    * @return The model index at given position. If there is no item
    * at the position or the position is not in the main widget an
    * invalid item will be returned.
    */
    QModelIndex indexAt(QPoint pos);

    /**
    * @return The ClientModel used by TabBox.
    */
    ClientModel* clientModel() const {
        return m_clientModel;
    }
    /**
    * @return The DesktopModel used by TabBox.
    */
    DesktopModel* desktopModel() {
        return m_desktopModel;
    }
    ClientItemDelegate* clientDelegate() const {
        return m_delegate;
    }
    ClientItemDelegate* additionalClientDelegate() const {
        return m_additionalClientDelegate;
    }
    DesktopItemDelegate* desktopDelegate() const {
        return m_desktopItemDelegate;
    }
    DesktopItemDelegate* additionalDesktopDelegate() const {
        return m_additionalDesktopDelegate;
    }

    void setPreview(bool preview);
    bool preview() const {
        return m_preview;
    }

    QRect selectedItem() const {
        return m_selectedItem;
    }
    void setSelectedItem(const QRect& rect) {
        m_selectedItem = rect;
    }

public slots:
    /**
    * Sets the current index in the two views.
    * @param index The new index
    */
    void setCurrentIndex(QModelIndex index);

private slots:
    /**
    * This slot reacts on a changed TabBoxConfig. It changes the used
    * item model and delegate if the TabBoxMode changed.
    */
    void configChanged();

private:
    /**
    * The main widget used to show all items
    */
    TabBoxMainView* m_tableView;
    TabBoxAdditionalView* m_additionalView;
    /**
    * Model for client items
    */
    ClientModel* m_clientModel;
    /**
    * Model for desktop items
    */
    DesktopModel* m_desktopModel;
    /**
    * Item delegate for client items
    */
    ClientItemDelegate* m_delegate;
    ClientItemDelegate* m_additionalClientDelegate;
    /**
    * Item delegate for desktop items
    */
    DesktopItemDelegate* m_desktopItemDelegate;
    DesktopItemDelegate* m_additionalDesktopDelegate;
    /**
    * Background Frame
    */
    Plasma::FrameSvg* m_frame;
    /**
    * The FrameSvg to render selected items
    */
    Plasma::FrameSvg* m_selectionFrame;
    /**
    * TabBoxView is a preview
    */
    bool m_preview;
    QPropertyAnimation* m_animation;
    QRect m_selectedItem;
    bool m_previewUpdate;

};

/**
* This class is the main widget of the TabBoxView.
* It is the "switching area" for TabBox items.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxMainView : public QTableView
{
public:
    TabBoxMainView(QWidget* parent = 0);
    ~TabBoxMainView();

    virtual QSize sizeHint() const;
    virtual QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
};
/**
* This class is the additional widget of the TabBoxView.
* It just displays the current item and can be turned off.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxAdditionalView : public QTableView
{
public:
    TabBoxAdditionalView(QWidget* parent = 0);
    ~TabBoxAdditionalView();

    virtual QSize sizeHint() const;
    virtual QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
    virtual void wheelEvent(QWheelEvent* event);
};

} // namespace Tabbox
} // namespace KWin

#endif // TABBOXVIEW_H
