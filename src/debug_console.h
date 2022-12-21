/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"
#include "input_event_spy.h"
#include <config-kwin.h>
#include <kwin_export.h>

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QVector>
#include <functional>
#include <memory>

class QTextEdit;

namespace KWaylandServer
{
class AbstractDataSource;
}

namespace Ui
{
class DebugConsole;
}

namespace KWin
{

class Window;
class X11Window;
class InternalWindow;
class Unmanaged;
class DebugConsoleFilter;
class WaylandWindow;

class KWIN_EXPORT DebugConsoleModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit DebugConsoleModel(QObject *parent = nullptr);
    ~DebugConsoleModel() override;

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private Q_SLOTS:
    void handleWindowAdded(Window *window);
    void handleWindowRemoved(Window *window);

private:
    template<class T>
    QModelIndex indexForWindow(int row, int column, const QVector<T *> &windows, int id) const;
    template<class T>
    QModelIndex indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const;
    template<class T>
    int propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const;
    QVariant propertyData(QObject *object, const QModelIndex &index, int role) const;
    template<class T>
    QVariant windowData(const QModelIndex &index, int role, const QVector<T *> windows, const std::function<QString(T *)> &toString) const;
    template<class T>
    void add(int parentRow, QVector<T *> &windows, T *window);
    template<class T>
    void remove(int parentRow, QVector<T *> &windows, T *window);
    WaylandWindow *waylandWindow(const QModelIndex &index) const;
    InternalWindow *internalWindow(const QModelIndex &index) const;
    X11Window *x11Window(const QModelIndex &index) const;
    Unmanaged *unmanaged(const QModelIndex &index) const;
    int topLevelRowCount() const;

    QVector<WaylandWindow *> m_waylandWindows;
    QVector<InternalWindow *> m_internalWindows;
    QVector<X11Window *> m_x11Windows;
    QVector<Unmanaged *> m_unmanageds;
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

    std::unique_ptr<Ui::DebugConsole> m_ui;
    std::unique_ptr<DebugConsoleFilter> m_inputFilter;
};

class SurfaceTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SurfaceTreeModel(QObject *parent = nullptr);
    ~SurfaceTreeModel() override;

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
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
    void touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    void touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    void touchUp(qint32 id, std::chrono::microseconds time) override;

    void pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override;
    void pinchGestureEnd(std::chrono::microseconds time) override;
    void pinchGestureCancelled(std::chrono::microseconds time) override;

    void swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    void swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override;
    void swipeGestureEnd(std::chrono::microseconds time) override;
    void swipeGestureCancelled(std::chrono::microseconds time) override;

    void switchEvent(SwitchEvent *event) override;

    void tabletToolEvent(TabletEvent *event) override;
    void tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time) override;
    void tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;
    void tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;
    void tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;

private:
    QTextEdit *m_textEdit;
};

class InputDeviceModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit InputDeviceModel(QObject *parent = nullptr);
    ~InputDeviceModel() override;

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private Q_SLOTS:
    void slotPropertyChanged();

private:
    void setupDeviceConnections(InputDevice *device);
    QList<InputDevice *> m_devices;
};

class DataSourceModel : public QAbstractItemModel
{
public:
    using QAbstractItemModel::QAbstractItemModel;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override
    {
        return parent.isValid() ? 0 : 2;
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setSource(KWaylandServer::AbstractDataSource *source);

private:
    KWaylandServer::AbstractDataSource *m_source = nullptr;
    QVector<QByteArray> m_data;
};
}
