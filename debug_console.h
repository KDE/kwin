/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DEBUG_CONSOLE_H
#define KWIN_DEBUG_CONSOLE_H

#include <kwin_export.h>
#include <config-kwin.h>
#include "input.h"
#include "input_event_spy.h"

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

class AbstractClient;
class X11Client;
class InternalClient;
class Unmanaged;
class DebugConsoleFilter;
class WaylandClient;

class KWIN_EXPORT DebugConsoleModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit DebugConsoleModel(QObject *parent = nullptr);
    ~DebugConsoleModel() override;


    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private Q_SLOTS:
    void handleClientAdded(AbstractClient *client);
    void handleClientRemoved(AbstractClient *client);

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
    WaylandClient *waylandClient(const QModelIndex &index) const;
    InternalClient *internalClient(const QModelIndex &index) const;
    X11Client *x11Client(const QModelIndex &index) const;
    Unmanaged *unmanaged(const QModelIndex &index) const;
    int topLevelRowCount() const;

    QVector<WaylandClient *> m_waylandClients;
    QVector<InternalClient*> m_internalClients;
    QVector<X11Client *> m_x11Clients;
    QVector<Unmanaged*> m_unmanageds;

};

class DebugConsoleDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DebugConsoleDelegate(QObject *parent = nullptr);
    ~DebugConsoleDelegate() override;

    QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class KWIN_EXPORT DebugConsole : public QWidget
{
    Q_OBJECT
public:
    DebugConsole();
    ~DebugConsole() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void initGLTab();
    void updateKeyboardTab();

    QScopedPointer<Ui::DebugConsole> m_ui;
    QScopedPointer<DebugConsoleFilter> m_inputFilter;
};

class SurfaceTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SurfaceTreeModel(QObject *parent = nullptr);
    ~SurfaceTreeModel() override;

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
};

class DebugConsoleFilter : public InputEventSpy
{
public:
    explicit DebugConsoleFilter(QTextEdit *textEdit);
    ~DebugConsoleFilter() override;

    void pointerEvent(MouseEvent *event) override;
    void wheelEvent(WheelEvent *event) override;
    void keyEvent(KeyEvent *event) override;
    void touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    void touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    void touchUp(qint32 id, quint32 time) override;

    void pinchGestureBegin(int fingerCount, quint32 time) override;
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override;
    void pinchGestureEnd(quint32 time) override;
    void pinchGestureCancelled(quint32 time) override;

    void swipeGestureBegin(int fingerCount, quint32 time) override;
    void swipeGestureUpdate(const QSizeF &delta, quint32 time) override;
    void swipeGestureEnd(quint32 time) override;
    void swipeGestureCancelled(quint32 time) override;

    void switchEvent(SwitchEvent *event) override;

    void tabletToolEvent(TabletEvent *event) override;
    void tabletToolButtonEvent(const QSet<uint> &pressedButtons) override;
    void tabletPadButtonEvent(const QSet<uint> &pressedButtons) override;
    void tabletPadStripEvent(int number, int position, bool isFinger) override;
    void tabletPadRingEvent(int number, int position, bool isFinger) override;

private:
    QTextEdit *m_textEdit;
};

namespace LibInput
{
class Device;
}

class InputDeviceModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit InputDeviceModel(QObject *parent = nullptr);
    ~InputDeviceModel() override;

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex & parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private:
    void setupDeviceConnections(LibInput::Device *device);
    QVector<LibInput::Device*> m_devices;
};

}

#endif
