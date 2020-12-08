/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_terminal.h"
// kwin
#include "logind.h"
#include "main.h"
#include "utils.h"
// Qt
#include <QDebug>
#include <QSocketNotifier>
// linux
#ifdef Q_OS_LINUX
#include <linux/major.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/sysmacros.h>
#endif
#ifdef Q_OS_FREEBSD
#include <sys/consio.h>
#endif
// system
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
// c++
#include <csignal>

#define RELEASE_SIGNAL SIGUSR1
#define ACQUISITION_SIGNAL SIGUSR2

namespace KWin
{

KWIN_SINGLETON_FACTORY(VirtualTerminal)

VirtualTerminal::VirtualTerminal(QObject *parent)
    : QObject(parent)
{
}

void VirtualTerminal::init()
{
    auto logind = LogindIntegration::self();
    if (logind->vt() != -1) {
        setup(logind->vt());
    }
    connect(logind, &LogindIntegration::virtualTerminalChanged, this, &VirtualTerminal::setup);
    if (logind->isConnected()) {
        logind->takeControl();
    } else {
        connect(logind, &LogindIntegration::connectedChanged, logind, &LogindIntegration::takeControl);
    }
}

VirtualTerminal::~VirtualTerminal()
{
    s_self = nullptr;
    closeFd();
}

static bool isTty(int fd)
{
    if (fd < 0) {
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return false;
    }
#ifdef Q_OS_LINUX
    // Not a TTY device or weird vt number, skip it
    if (major(st.st_rdev) != TTY_MAJOR || minor (st.st_rdev) <= 0 || minor(st.st_rdev) >= 64) {
        return false;
    }
#endif
    // FreeBSD doesn't have a notion of major device number, so nothing
    //   to check. isatty() might not do the trick.
    return true;
}

void VirtualTerminal::setup(int vtNr)
{
    if (m_vt != -1) {
        return;
    }
    if (vtNr == -1) {
        // error condition
        return;
    }
    QString ttyName = QStringLiteral(KWIN_TTY_PREFIX "%1").arg(vtNr);

    m_vt = open(ttyName.toUtf8().constData(), O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (m_vt < 0) {
        qCWarning(KWIN_CORE) << "Failed to open tty" << vtNr;
        return;
    }
    if (!isTty(m_vt)) {
        qCWarning(KWIN_CORE) << vtNr << " is no tty";
        closeFd();
        return;
    }
    if (ioctl(m_vt, KDSETMODE, KD_GRAPHICS) < 0) {
        qCWarning(KWIN_CORE()) << "Failed to set tty " << vtNr << " in graphics mode";
        closeFd();
        return;
    }
    if (!createSignalHandler()) {
        qCWarning(KWIN_CORE) << "Failed to create signalfd";
        closeFd();
        return;
    }
    vt_mode mode = {
        VT_PROCESS,
        0,
        RELEASE_SIGNAL,
        ACQUISITION_SIGNAL,
        0
    };
    if (ioctl(m_vt, VT_SETMODE, &mode) < 0) {
        qCWarning(KWIN_CORE) << "Failed to take over virtual terminal";
        closeFd();
        return;
    }
    m_vtNumber = vtNr;
    setActive(true);
    emit kwinApp()->virtualTerminalCreated();
}

void VirtualTerminal::closeFd()
{
    if (m_vt < 0) {
        return;
    }
    if (m_notifier) {
        const int sfd = m_notifier->socket();
        delete m_notifier;
        m_notifier = nullptr;
        close(sfd);
    }
    close(m_vt);
    m_vt = -1;
}

bool VirtualTerminal::createSignalHandler()
{
    if (m_notifier) {
        return false;
    }
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, RELEASE_SIGNAL);
    sigaddset(&mask, ACQUISITION_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    const int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this,
        [this] {
            if (m_vt < 0) {
                return;
            }
            while (true) {
                signalfd_siginfo sigInfo;
                if (read(m_notifier->socket(), &sigInfo, sizeof(sigInfo)) != sizeof(sigInfo)) {
                    break;
                }
                switch (sigInfo.ssi_signo) {
                case RELEASE_SIGNAL:
                    setActive(false);
                    ioctl(m_vt, VT_RELDISP, 1);
                    break;
                case ACQUISITION_SIGNAL:
                    ioctl(m_vt, VT_RELDISP, VT_ACKACQ);
                    setActive(true);
                    break;
                }
            }
        }
    );
    return true;
}

void VirtualTerminal::activate(int vt)
{
    if (m_vt < 0) {
        return;
    }
    if (vt == m_vtNumber) {
        return;
    }
    ioctl(m_vt, VT_ACTIVATE, vt);
    setActive(false);
}

void VirtualTerminal::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    emit activeChanged(m_active);
}

}
