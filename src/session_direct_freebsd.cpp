/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_direct.h"
#include "utils.h"

#include <QScopeGuard>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/consio.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/kbio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DRM_IOCTL_SET_MASTER
#define DRM_IOCTL_SET_MASTER _IO('d', 0x1e)
#endif
#ifndef DRM_IOCTL_DROP_MASTER
#define DRM_IOCTL_DROP_MASTER _IO('d', 0x1f)
#endif

#define DRM_MAJOR 226

#ifndef KDSKBMUTE
#define KDSKBMUTE 0x4B51
#endif

namespace KWin
{

DirectSession *DirectSession::create(QObject *parent)
{
    DirectSession *session = new DirectSession(parent);
    if (session->setupTerminal()) {
        return session;
    }

    delete session;
    return nullptr;
}

DirectSession::DirectSession(QObject *parent)
    : Session(parent)
{
    const QString seat = qEnvironmentVariable("XDG_SEAT");
    if (!seat.isEmpty()) {
        m_seat = seat;
    } else {
        m_seat = QStringLiteral("seat0");
    }
}

DirectSession::~DirectSession()
{
    if (m_ttyFd == -1) {
        return;
    }
    restoreTerminal();
    close(m_ttyFd);
    close(m_signalNotifier->socket());
}

bool DirectSession::setupTerminal()
{
    if (m_seat != QStringLiteral("seat0")) {
        qCDebug(KWIN_CORE) << "Skipping VT initialization";
        return true;
    }

    if (ioctl(0, VT_OPENQRY, &m_terminal)) {
        qCWarning(KWIN_CORE, "Failed to get unused VT: %s", strerror(errno));
        return false;
    }

    const QByteArray ttyPath = "/dev/ttyv" + QByteArray::number(m_terminal - 1);

    int fd = open(ttyPath.constData(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd == -1) {
        qCWarning(KWIN_CORE, "Cannot open %s: %s", ttyPath.constData(), strerror(errno));
        return false;
    }
    auto ttyCleanup = qScopeGuard([&fd]() { close(fd); });

    int kdMode;
    if (ioctl(fd, KDGETMODE, &kdMode)) {
        qCWarning(KWIN_CORE, "Failed to get the keyboard mode: %s", strerror(errno));
        return false;
    }
    if (kdMode != KD_TEXT) {
        qCWarning(KWIN_CORE) << "tty is already in graphics mode";
    }

    ioctl(fd, VT_ACTIVATE, m_terminal);
    ioctl(fd, VT_WAITACTIVE, m_terminal);

    if (ioctl(fd, KDGKBMODE, &m_keyboardMode)) {
        qCWarning(KWIN_CORE, "Failed to read keyboard mode: %s", strerror(errno));
        return false;
    }

    if (ioctl(fd, KDSKBMUTE, 1) && ioctl(fd, KDSKBMODE, K_CODE)) {
        qCWarning(KWIN_CORE, "Failed to set K_CODE keyboard mode: %s", strerror(errno));
        return false;
    }

    if (ioctl(fd, KDSETMODE, KD_GRAPHICS)) {
        qCWarning(KWIN_CORE, "Failed to set graphics mode on tty: %s", strerror(errno));
        return false;
    }

    vt_mode virtualTerminalMode = {};
    virtualTerminalMode.mode = VT_PROCESS;
    virtualTerminalMode.relsig = SIGUSR1;
    virtualTerminalMode.acqsig = SIGUSR2;
    virtualTerminalMode.frsig = SIGUSR1; // unused, but still needs to be set

    if (ioctl(fd, VT_SETMODE, &virtualTerminalMode)) {
        qCWarning(KWIN_CORE, "Failed to take control of vt: %s", strerror(errno));
        return false;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        qCWarning(KWIN_CORE) << "Failed to block acquire and release tty signals";
        return false;
    }

    const int queueFd = kqueue();
    if (queueFd < 0) {
        qCWarning(KWIN_CORE, "Failed to create a signal queue: %s", strerror(errno));
        return false;
    }
    auto queueCleanup = qScopeGuard([&queueFd]() { close(queueFd); });

    struct kevent signalEvents[2];
    EV_SET(&signalEvents[0], SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, nullptr);
    EV_SET(&signalEvents[1], SIGUSR2, EVFILT_SIGNAL, EV_ADD, 0, 0, nullptr);
    if (kevent(queueFd, signalEvents, 2, nullptr, 0, nullptr) == -1) {
        qCWarning(KWIN_CORE, "Failed to add signals to kqueue: %s", strerror(errno));
        return false;
    }
    for (const struct kevent &event : signalEvents) {
        if (event.flags & EV_ERROR) {
            qCWarning(KWIN_CORE, "Failed to add %s to kqueue: %s",
                      strsignal(event.ident), strerror(event.data));
            return false;
        }
    }
    fcntl(queueFd, F_SETFD, FD_CLOEXEC);

    m_signalNotifier = new QSocketNotifier(queueFd, QSocketNotifier::Read, this);
    connect(m_signalNotifier, &QSocketNotifier::activated,
            this, &DirectSession::processSignals);

    m_isActive = true;
    m_ttyFd = fd;

    queueCleanup.dismiss();
    ttyCleanup.dismiss();

    return true;
}

void DirectSession::restoreTerminal()
{
    vt_mode virtualTerminalMode = {};

    if (ioctl(m_ttyFd, KDSKBMUTE, 0) && ioctl(m_ttyFd, KDSKBMODE, m_keyboardMode)) {
        qCWarning(KWIN_CORE, "Failed to restore keyboard mode: %s", strerror(errno));
    }

    if (ioctl(m_ttyFd, KDSETMODE, KD_TEXT)) {
        qCWarning(KWIN_CORE, "Failed to set KD_TEXT mode on tty: %s", strerror(errno));
    }

    virtualTerminalMode.mode = VT_AUTO;
    if (ioctl(m_ttyFd, VT_SETMODE, &virtualTerminalMode)) {
        qCWarning(KWIN_CORE, "Failed to reset VT handling: %s", strerror(errno));
    }
}

bool DirectSession::isActive() const
{
    return m_isActive;
}

DirectSession::Capabilities DirectSession::capabilities() const
{
    return Capability::SwitchTerminal;
}

QString DirectSession::seat() const
{
    return m_seat;
}

uint DirectSession::terminal() const
{
    return m_terminal;
}

static void drmSetMasterInternal(int fd)
{
    if (ioctl(fd, DRM_IOCTL_SET_MASTER, 0) == -1) {
        qCWarning(KWIN_CORE) << "ioctl(DRM_IOCTL_SET_MASTER) failed:" << strerror(errno);
    }
}

static void drmDropMasterInternal(int fd)
{
    if (ioctl(fd, DRM_IOCTL_DROP_MASTER, 0) == -1) {
        qCWarning(KWIN_CORE) << "ioctl(DRM_IOCTL_DROP_MASTER) failed:" << strerror(errno);
    }
}

int DirectSession::openRestricted(const QString &fileName)
{
    const int fd = open(fileName.toUtf8().constData(),
                        O_RDWR | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        qCWarning(KWIN_CORE) << "open() failed:" << strerror(errno);
        return -1;
    }

    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        close(fd);
        qCWarning(KWIN_CORE) << "fstat() failed:" << strerror(errno);
        return -1;
    }

    DirectSessionDevice device;
    device.fd = fd;
    device.id = buf.st_rdev;
    m_devices.append(device);

    if (major(device.id) == DRM_MAJOR) {
        drmSetMasterInternal(device.fd);
    }

    return fd;
}

void DirectSession::closeRestricted(int fileDescriptor)
{
    auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const DirectSessionDevice &device) {
        return device.fd == fileDescriptor;
    });
    if (it == m_devices.end()) {
        close(fileDescriptor);
        return;
    }

    const DirectSessionDevice device = *it;
    m_devices.erase(it);

    if (major(device.id) == DRM_MAJOR) {
        drmDropMasterInternal(device.fd);
    }
    close(fileDescriptor);
}

void DirectSession::switchTo(uint terminal)
{
    if (m_seat == QStringLiteral("seat0")) {
        ioctl(m_ttyFd, VT_ACTIVATE, terminal);
    }
}

void DirectSession::updateActive(bool active)
{
    if (m_isActive != active) {
        m_isActive = active;
        emit activeChanged(active);
    }
}

void DirectSession::processSignals()
{
    const int queueFd = m_signalNotifier->socket();

    while (true) {
        struct timespec ts = { 0, 0 };
        struct kevent kev;
        const int eventCount = kevent(queueFd, nullptr, 0, &kev, 1, &ts);

        if (eventCount < 0) {
            qCWarning(KWIN_CORE, "kevent() failed: %s", strerror(errno));
            return;
        } else if (eventCount == 0) {
            break;
        }

        switch (kev.ident) {
        case SIGUSR1:
            qCDebug(KWIN_CORE) << "Releasing virtual terminal" << m_terminal;
            updateActive(false);
            for (const DirectSessionDevice &device : qAsConst(m_devices)) {
                if (major(device.id) == DRM_MAJOR) {
                    drmDropMasterInternal(device.fd);
                }
            }
            ioctl(m_ttyFd, VT_RELDISP, 1);
            break;
        case SIGUSR2:
            qCDebug(KWIN_CORE) << "Acquiring virtual terminal" << m_terminal;
            ioctl(m_ttyFd, VT_RELDISP, VT_ACKACQ);
            for (const DirectSessionDevice &device : qAsConst(m_devices)) {
                if (major(device.id) == DRM_MAJOR) {
                    drmSetMasterInternal(device.fd);
                }
            }
            updateActive(true);
            break;
        }
    }
}

} // namespace KWin
