/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_DEBUG_CONSOLE_H
#define KWIN_DEBUG_CONSOLE_H

#include <kwin_export.h>
#include <config-kwin.h>
#include "input.h"

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QVector>

class QTextEdit;

namespace Ui
{
class DebugConsole;
}

namespace KWin
{

class Client;
class ShellClient;
class Unmanaged;
class DebugConsoleFilter;

class KWIN_EXPORT DebugConsoleModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit DebugConsoleModel(QObject *parent = nullptr);
    virtual ~DebugConsoleModel();


    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private:
    template <class T>
    QModelIndex indexForClient(int row, int column, const QVector<T*> &clients, int id) const;
    template <class T>
    QModelIndex indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex&) const) const;
    template <class T>
    int propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex&) const) const;
    QVariant propertyData(QObject *object, const QModelIndex &index, int role) const;
    template <class T>
    QVariant clientData(const QModelIndex &index, int role, const QVector<T*> clients) const;
    template <class T>
    void add(int parentRow, QVector<T*> &clients, T *client);
    template <class T>
    void remove(int parentRow, QVector<T*> &clients, T *client);
    ShellClient *shellClient(const QModelIndex &index) const;
    ShellClient *internalClient(const QModelIndex &index) const;
    Client *x11Client(const QModelIndex &index) const;
    Unmanaged *unmanaged(const QModelIndex &index) const;
    int topLevelRowCount() const;

    QVector<ShellClient*> m_shellClients;
    QVector<ShellClient*> m_internalClients;
    QVector<Client*> m_x11Clients;
    QVector<Unmanaged*> m_unmanageds;

};

class DebugConsoleDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DebugConsoleDelegate(QObject *parent = nullptr);
    virtual ~DebugConsoleDelegate();

    QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class KWIN_EXPORT DebugConsole : public QWidget
{
    Q_OBJECT
public:
    DebugConsole();
    virtual ~DebugConsole();

private:
    void initGLTab();

    QScopedPointer<Ui::DebugConsole> m_ui;
    QScopedPointer<DebugConsoleFilter> m_inputFilter;
};

class SurfaceTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SurfaceTreeModel(QObject *parent = nullptr);
    virtual ~SurfaceTreeModel();

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
};

class DebugConsoleFilter : public InputEventFilter
{
public:
    explicit DebugConsoleFilter(QTextEdit *textEdit);
    virtual ~DebugConsoleFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(quint32 id, quint32 time) override;

    bool pinchGestureBegin(int fingerCount, quint32 time) override;
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override;
    bool pinchGestureEnd(quint32 time) override;
    bool pinchGestureCancelled(quint32 time) override;

    bool swipeGestureBegin(int fingerCount, quint32 time) override;
    bool swipeGestureUpdate(const QSizeF &delta, quint32 time) override;
    bool swipeGestureEnd(quint32 time) override;
    bool swipeGestureCancelled(quint32 time) override;

private:
    QTextEdit *m_textEdit;
};

#if HAVE_INPUT

namespace LibInput
{
class Device;
}

class InputDeviceModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit InputDeviceModel(QObject *parent = nullptr);
    virtual ~InputDeviceModel();

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private:
    void setupDeviceConnections(LibInput::Device *device);
    QVector<LibInput::Device*> m_devices;
};
#endif

}

#endif
