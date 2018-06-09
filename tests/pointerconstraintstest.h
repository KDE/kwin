/********************************************************************
Copyright 2018 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef POINTERCONSTRAINTSTEST_H
#define POINTERCONSTRAINTSTEST_H

#include <QObject>
#include <QQuickView>

#include <xcb/xcb.h>

namespace KWayland {
namespace Client {

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
    Backend(QObject *parent = nullptr) : QObject(parent) {}

    Q_PROPERTY(int mode READ mode CONSTANT)
    Q_PROPERTY(bool errorsAllowed READ errorsAllowed WRITE setErrorsAllowed NOTIFY errorsAllowedChanged)

    virtual void init(QQuickView *view) {
        m_view = view;
    }
    int mode() const {
        return (int)m_mode;
    }
    bool errorsAllowed() const {
        return m_errorsAllowed;
    }
    void setErrorsAllowed(bool set) {
        if (m_errorsAllowed == set) {
            return;
        }
        m_errorsAllowed = set;
        Q_EMIT errorsAllowedChanged();
    }

    Q_INVOKABLE virtual void lockRequest(bool persistent = true, QRect region = QRect()) {
        Q_UNUSED(persistent);
        Q_UNUSED(region);
    }
    Q_INVOKABLE virtual void unlockRequest() {}

    Q_INVOKABLE virtual void confineRequest(bool persistent = true, QRect region = QRect()) {
        Q_UNUSED(persistent);
        Q_UNUSED(region);
    }
    Q_INVOKABLE virtual void unconfineRequest() {}
    Q_INVOKABLE virtual void hideAndConfineRequest(bool confineBeforeHide = false) {
        Q_UNUSED(confineBeforeHide);
    }
    Q_INVOKABLE virtual void undoHideRequest() {}

Q_SIGNALS:
    void confineChanged(bool confined);
    void lockChanged(bool locked);
    void errorsAllowedChanged();

protected:
    enum class Mode {
        Wayland = 0,
        X = 1
    };

    QQuickView* view() const {
        return m_view;
    }
    void setMode(Mode set) {
        m_mode = set;
    }

private:
    QQuickView *m_view;
    Mode m_mode;
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
