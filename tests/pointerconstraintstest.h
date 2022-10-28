/*
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef POINTERCONSTRAINTSTEST_H
#define POINTERCONSTRAINTSTEST_H

#include <QObject>
#include <QQuickView>

#include <xcb/xcb.h>

namespace KWayland
{
namespace Client
{

class ConnectionThread;
class Registry;
class Compositor;
class Seat;
class Pointer;
class PointerConstraints;
class LockedPointer;
class ConfinedPointer;

}
}

class MainWindow;

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_PROPERTY(int mode READ mode CONSTANT)
    Q_PROPERTY(bool lockHint MEMBER m_lockHint NOTIFY lockHintChanged)
    Q_PROPERTY(bool errorsAllowed READ errorsAllowed WRITE setErrorsAllowed NOTIFY errorsAllowedChanged)

    virtual void init(QQuickView *view)
    {
        m_view = view;
    }
    int mode() const
    {
        return (int)m_mode;
    }

    bool lockHint() const
    {
        return m_lockHint;
    }
    bool errorsAllowed() const
    {
        return m_errorsAllowed;
    }
    void setErrorsAllowed(bool set)
    {
        if (m_errorsAllowed == set) {
            return;
        }
        m_errorsAllowed = set;
        Q_EMIT errorsAllowedChanged();
    }

    Q_INVOKABLE virtual void lockRequest(bool persistent = true, QRect region = QRect())
    {
    }
    Q_INVOKABLE virtual void unlockRequest()
    {
    }

    Q_INVOKABLE virtual void confineRequest(bool persistent = true, QRect region = QRect())
    {
    }
    Q_INVOKABLE virtual void unconfineRequest()
    {
    }
    Q_INVOKABLE virtual void hideAndConfineRequest(bool confineBeforeHide = false)
    {
    }
    Q_INVOKABLE virtual void undoHideRequest()
    {
    }

Q_SIGNALS:
    void confineChanged(bool confined);
    void lockChanged(bool locked);
    void lockHintChanged();
    void errorsAllowedChanged();
    void forceSurfaceCommit();

protected:
    enum class Mode {
        Wayland = 0,
        X = 1
    };

    QQuickView *view() const
    {
        return m_view;
    }
    void setMode(Mode set)
    {
        m_mode = set;
    }

private:
    QQuickView *m_view;
    Mode m_mode;

    bool m_lockHint = true;
    bool m_errorsAllowed = false;
};

class WaylandBackend : public Backend
{
    Q_OBJECT
public:
    WaylandBackend(QObject *parent = nullptr);

    void init(QQuickView *view) override;

    void lockRequest(bool persistent, QRect region) override;
    void unlockRequest() override;

    void confineRequest(bool persistent, QRect region) override;
    void unconfineRequest() override;

private:
    void setupRegistry(KWayland::Client::Registry *registry);

    bool isLocked();
    bool isConfined();

    void cleanupLock();
    void cleanupConfine();

    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Pointer *m_pointer = nullptr;
    KWayland::Client::PointerConstraints *m_pointerConstraints = nullptr;

    KWayland::Client::LockedPointer *m_lockedPointer = nullptr;
    bool m_lockedPointerPersistent = false;
    KWayland::Client::ConfinedPointer *m_confinedPointer = nullptr;
    bool m_confinedPointerPersistent = false;
};

class XBackend : public Backend
{
    Q_OBJECT
public:
    XBackend(QObject *parent = nullptr);

    void init(QQuickView *view) override;

    void lockRequest(bool persistent, QRect region) override;
    void unlockRequest() override;

    void confineRequest(bool persistent, QRect region) override;
    void unconfineRequest() override;

    void hideAndConfineRequest(bool confineBeforeHide) override;
    void undoHideRequest() override;

private:
    bool tryConfine(int &error);
    xcb_connection_t *m_xcbConn = nullptr;
};

#endif
