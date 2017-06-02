/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_TABBOX_LAYOUTPREVIEW_H
#define KWIN_TABBOX_LAYOUTPREVIEW_H

#include <KService>
#include <QAbstractListModel>
#include <QQuickView>
#include <QRect>

namespace KWin
{

namespace TabBox
{

class SwitcherItem;

class LayoutPreview : public QObject
{
    Q_OBJECT
public:
    explicit LayoutPreview(const QString &path, QObject *parent = nullptr);
    virtual ~LayoutPreview();

    virtual bool eventFilter(QObject *object, QEvent *event) override;
private:
    SwitcherItem *m_item;
};

class ExampleClientModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ExampleClientModel(QObject *parent = nullptr);
    virtual ~ExampleClientModel();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    Q_INVOKABLE QString longestCaption() const;

private:
    void init();
    QList<KService::Ptr> m_services;
    KService::Ptr m_fileManager;
    KService::Ptr m_browser;
    KService::Ptr m_email;
    KService::Ptr m_systemSettings;
};


class SwitcherItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *model READ model NOTIFY modelChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY screenGeometryChanged)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool allDesktops READ isAllDesktops NOTIFY allDesktopsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

    /**
     * The main QML item that will be displayed in the Dialog
     */
    Q_PROPERTY(QObject *item READ item WRITE setItem NOTIFY itemChanged)

    Q_CLASSINFO("DefaultProperty", "item")
public:
    SwitcherItem(QObject *parent = nullptr);
    virtual ~SwitcherItem();

    QAbstractItemModel *model() const;
    QRect screenGeometry() const;
    bool isVisible() const;
    bool isAllDesktops() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    QObject *item() const;
    void setItem(QObject *item);

    void setVisible(bool visible);
    void incrementIndex();
    void decrementIndex();

Q_SIGNALS:
    void visibleChanged();
    void currentIndexChanged(int index);
    void modelChanged();
    void allDesktopsChanged();
    void screenGeometryChanged();
    void itemChanged();

private:
    QAbstractItemModel *m_model;
    QObject *m_item;
    int m_currentIndex;
    bool m_visible;
};

inline QAbstractItemModel *SwitcherItem::model() const
{
    return m_model;
}

inline bool SwitcherItem::isVisible() const
{
    return m_visible;
}

inline bool SwitcherItem::isAllDesktops() const
{
    return true;
}

inline int SwitcherItem::currentIndex() const
{
    return m_currentIndex;
}

inline QObject *SwitcherItem::item() const
{
    return m_item;
}

} // namespace TabBox
} // namespace KWin

#endif // KWIN_TABBOX_LAYOUTPREVIEW_H
