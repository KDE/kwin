/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-kwin.h"

#include "input.h"
#include "input_event_spy.h"
#include <kwin_export.h>

#include <QAbstractItemModel>
#include <QList>
#include <QListWidget>
#include <QStyledItemDelegate>

#include <functional>
#include <memory>

class QLabel;
class QPushButton;
class QTextEdit;

namespace Ui
{
class DebugConsole;
}

namespace KWin
{

class AbstractDataSource;
class Window;
class X11Window;
class InternalWindow;
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
    QModelIndex indexForWindow(int row, int column, const QList<T *> &windows, int id) const;
    template<class T>
    QModelIndex indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const;
    template<class T>
    int propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const;
    QVariant propertyData(QObject *object, const QModelIndex &index, int role) const;
    template<class T>
    QVariant windowData(const QModelIndex &index, int role, const QList<T *> windows, const std::function<QString(T *)> &toString) const;
    template<class T>
    void add(int parentRow, QList<T *> &windows, T *window);
    template<class T>
    void remove(int parentRow, QList<T *> &windows, T *window);
    WaylandWindow *waylandWindow(const QModelIndex &index) const;
    InternalWindow *internalWindow(const QModelIndex &index) const;
    X11Window *x11Window(const QModelIndex &index) const;
    X11Window *unmanaged(const QModelIndex &index) const;
    int topLevelRowCount() const;

    QList<WaylandWindow *> m_waylandWindows;
    QList<InternalWindow *> m_internalWindows;
    QList<X11Window *> m_x11Windows;
    QList<X11Window *> m_unmanageds;
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

class DebugConsoleFilter : public InputEventSpy
{
public:
    explicit DebugConsoleFilter(QTextEdit *textEdit);
    ~DebugConsoleFilter() override;

    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    void pointerAxis(PointerAxisEvent *event) override;
    void keyboardKey(KeyboardKeyEvent *event) override;
    void touchDown(TouchDownEvent *event) override;
    void touchMotion(TouchMotionEvent *event) override;
    void touchUp(TouchUpEvent *event) override;

    void pinchGestureBegin(PointerPinchGestureBeginEvent *event) override;
    void pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override;
    void pinchGestureEnd(PointerPinchGestureEndEvent *event) override;
    void pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override;

    void swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override;
    void swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override;
    void swipeGestureEnd(PointerSwipeGestureEndEvent *event) override;
    void swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override;

    void holdGestureBegin(PointerHoldGestureBeginEvent *event) override;
    void holdGestureEnd(PointerHoldGestureEndEvent *event) override;
    void holdGestureCancelled(PointerHoldGestureCancelEvent *event) override;

    void switchEvent(SwitchEvent *event) override;

    void tabletToolProximityEvent(TabletToolProximityEvent *event) override;
    void tabletToolAxisEvent(TabletToolAxisEvent *event) override;
    void tabletToolTipEvent(TabletToolTipEvent *event) override;
    void tabletToolButtonEvent(TabletToolButtonEvent *event) override;
    void tabletPadButtonEvent(TabletPadButtonEvent *event) override;
    void tabletPadStripEvent(TabletPadStripEvent *event) override;
    void tabletPadRingEvent(TabletPadRingEvent *event) override;
    void tabletPadDialEvent(TabletPadDialEvent *event) override;

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

    void setSource(AbstractDataSource *source);

private:
    AbstractDataSource *m_source = nullptr;
    QList<QByteArray> m_data;
};

class DebugConsoleEffectItem : public QWidget
{
    Q_OBJECT

public:
    explicit DebugConsoleEffectItem(const QString &name, bool loaded, QWidget *parent = nullptr);

private:
    QString m_name;
    bool m_loaded = false;
};

class DebugConsoleEffectsTab : public QListWidget
{
    Q_OBJECT

public:
    explicit DebugConsoleEffectsTab(QWidget *parent = nullptr);
};

} // namespace KWin
