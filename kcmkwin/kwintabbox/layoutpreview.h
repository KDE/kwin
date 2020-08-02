/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
    ~LayoutPreview() override;

    bool eventFilter(QObject *object, QEvent *event) override;
private:
    SwitcherItem *m_item;
};

class ExampleClientModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum {
        CaptionRole = Qt::UserRole + 1,
        MinimizedRole,
        DesktopNameRole,
        IconRole,
        WindowIdRole
    };

    explicit ExampleClientModel(QObject *parent = nullptr);
    ~ExampleClientModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
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
    ~SwitcherItem() override;

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
