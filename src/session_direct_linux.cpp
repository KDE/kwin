/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_direct.h"
#include "utils.h"

#include <QScopeGuard>

#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

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

    const char *ttyPath = "/dev/tty";

    int fd = open(ttyPath, O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        qCWarning(KWIN_CORE, "Cannot open %s: %s", ttyPath, strerror(errno));
        return false;
    }
    auto cleanup = qScopeGuard([&fd]() { close(fd); });

    vt_stat virtualTerminalStat;
    if (ioctl(fd, VT_GETSTATE, &virtualTerminalStat)) {
        qCWarning(KWIN_CORE) << "Failed to get current tty number";
        return false;
    }

    m_terminal = virtualTerminalStat.v_active;

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

    if (ioctl(fd, KDSKBMUTE, 1) && ioctl(fd, KDSKBMODE, K_OFF)) {
        qCWarning(KWIN_CORE, "Failed to set K_OFF keyboard mode: %s", strerror(errno));
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

    const int signalFd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signalFd == -1) {
        qCWarning(KWIN_CORE, "Failed to create signalfd for vt handler");
        return false;
    }
    m_signalNotifier = new QSocketNotifier(signalFd, QSocketNotifier::Read, this);
    connect(m_signalNotifier, &QSocketNotifier::activated,
            this, &DirectSession::processSignals);

    m_isActive = true;
    m_ttyFd = fd;
    cleanup.dismiss();

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
        qCWarning(KWIN_CORE) << "fstat() failed:" << strerror(errno);
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
    const int signalFd = m_signalNotifier->socket();

    while (true) {
        signalfd_siginfo info;

        const ssize_t readCount = read(signalFd, &info, sizeof(info));
        if (readCount != sizeof(info)) {
            break;
        }

        switch (info.ssi_signo) {
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
