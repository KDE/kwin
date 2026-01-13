/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gamecontroller.h"
#include "gamecontroller_logging.h"

#include "input.h"

#include <QDebug>
#include <QKeySequence>

namespace KWin
{

void LibevdevDeleter::operator()(libevdev *dev) const
{
    if (dev) {
        libevdev_free(dev);
    }
}

GameController::GameController(const QString &path, FileDescriptor &&fd, libevdev *evdev)
    : m_path(path)
    , m_fd(std::move(fd))
    , m_evdev(evdev)
{
    Q_ASSERT(m_fd.isValid());

    m_notifier = std::make_unique<QSocketNotifier>(m_fd.get(), QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &GameController::handleEvdevEvent);

    libevdev_set_clock_id(m_evdev.get(), CLOCK_MONOTONIC);

    qCDebug(KWIN_GAMECONTROLLER) << "Connected to GameController:" << libevdev_get_name(m_evdev.get()) << "at" << m_path;

    // default assumption: when KWin opens the device, no other process is using it
    enableInputEmulation();
}

GameController::~GameController()
{
    if (m_inputdevice) {
        input()->removeInputDevice(m_inputdevice.get());
    }
}

const QString &GameController::path() const
{
    return m_path;
}

int GameController::fd() const
{
    return m_fd.get();
}

libevdev *GameController::evdev() const
{
    return m_evdev.get();
}

void GameController::increaseUsageCount()
{
    m_usageCount++;
    if (m_usageCount == 1) {
        input()->removeInputDevice(m_inputdevice.get());
        m_inputdevice.reset();
    }
}

void GameController::decreaseUsageCount()
{
    if (m_usageCount == 0) {
        // It's not possible to reliably find out how many processes had
        // the input node open before this plugin started watching it,
        // so we need to guard against m_usageCount becoming negative
        return;
    }
    m_usageCount--;
    if (m_usageCount == 0) {
        enableInputEmulation();
    }
}

void GameController::enableInputEmulation()
{
    m_inputdevice = std::make_unique<EmulatedInputDevice>();
    input()->addInputDevice(m_inputdevice.get());

    m_inputdevice->setMaxAbs(libevdev_get_abs_maximum(m_evdev.get(), ABS_RX));
    m_inputdevice->setMinAbs(libevdev_get_abs_minimum(m_evdev.get(), ABS_RX));
}

int GameController::usageCount()
{
    return m_usageCount;
}

void GameController::handleEvdevEvent()
{
    input_event ev;
    int rc;

    while ((rc = libevdev_next_event(m_evdev.get(), LIBEVDEV_READ_FLAG_NORMAL, &ev)) != -EAGAIN) {
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // SYN_DROPPED recovery: drain sync events
            while (libevdev_next_event(m_evdev.get(), LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC) {
                // Recovered / Synced. Back to main loop
            }
            continue;
        }

        if (rc == -ENODEV) {
            // device got removed, we can safely ignore this
            return;
        } else if (rc < 0) {
            qCWarning(KWIN_GAMECONTROLLER) << "Error reading event:" << strerror(-rc);
            break;
        }

        logEvent(&ev);

        input()->simulateUserActivity();

        if (m_usageCount == 0 || isTestEnvironment) {
            m_inputdevice->emulateInputDevice(ev);
        }
    }
}

void GameController::logEvent(input_event *ev)
{
    if (!KWIN_GAMECONTROLLER().isDebugEnabled()) {
        return;
    }

    const qint64 timestamp = ev->time.tv_sec * 1000 + ev->time.tv_usec / 1000;

    if (ev->type == EV_SYN) {
        qCDebug(KWIN_GAMECONTROLLER) << "Device:" << libevdev_get_name(m_evdev.get()) << "time(ms):" << timestamp << "Event: -------------- SYN_REPORT ------------";
    } else {
        // type, code, and value
        qCDebug(KWIN_GAMECONTROLLER) << "Device:" << libevdev_get_name(m_evdev.get()) << "time(ms):" << timestamp << "Event:"
                                     << "type:" << ev->type
                                     << "(" << libevdev_event_type_get_name(ev->type) << "),"
                                     << "code:" << ev->code
                                     << "(" << libevdev_event_code_get_name(ev->type, ev->code) << "),"
                                     << "value:" << ev->value;
    }
}

} // namespace KWin

#include "moc_gamecontroller.cpp"
