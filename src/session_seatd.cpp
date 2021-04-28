/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_seatd.h"
#include "utils.h"

#include <unistd.h>

#include <QSocketNotifier>

namespace KWin
{

SeatdSession *SeatdSession::create(QObject *parent)
{
    SeatdSession *session = new SeatdSession(parent);
    if (session->initialize()) {
        return session;
    }

    delete session;
    return nullptr;
}

SeatdSession::SeatdSession(QObject *parent)
    : Session(parent)
{
}

SeatdSession::~SeatdSession()
{
    if (m_seat) {
        libseat_close_seat(m_seat);
    }
}

static void logHandler(libseat_log_level level, const char *format, va_list args)
{
    char buffer[1024];
    if (std::vsnprintf(buffer, 1023, format, args) <= 0) {
        return;
    }

    switch (level) {
    case LIBSEAT_LOG_LEVEL_DEBUG:
        qCDebug(KWIN_CORE) << "libseat:" << buffer;
        break;
    case LIBSEAT_LOG_LEVEL_INFO:
        qCInfo(KWIN_CORE) << "libseat:" << buffer;
        break;
    case LIBSEAT_LOG_LEVEL_ERROR:
        qCCritical(KWIN_CORE) << "libseat:" << buffer;
        break;
    default:
        qCDebug(KWIN_CORE) << "libseat:" << buffer;
    }
}

bool SeatdSession::initialize()
{
    static libseat_seat_listener seatListener = {
        .enable_seat = &handleEnableSeat,
        .disable_seat = &handleDisableSeat,
    };

    m_seat = libseat_open_seat(&seatListener, this);
    if (!m_seat) {
        return false;
    }

    libseat_set_log_handler(logHandler);
    libseat_set_log_level(LIBSEAT_LOG_LEVEL_ERROR);

    auto notifier = new QSocketNotifier(libseat_get_fd(m_seat), QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &SeatdSession::dispatchEvents);

    return true;
}

bool SeatdSession::isActive() const
{
    return m_isActive;
}

SeatdSession::Capabilities SeatdSession::capabilities() const
{
    return Capability::SwitchTerminal;
}

QString SeatdSession::seat() const
{
    return QString::fromUtf8(libseat_seat_name(m_seat));
}

uint SeatdSession::terminal() const
{
    return 0;
}

int SeatdSession::openRestricted(const QString &fileName)
{
    int fileDescriptor;
    const int deviceId = libseat_open_device(m_seat, fileName.toUtf8().constData(), &fileDescriptor);
    if (deviceId == -1) {
        qCDebug(KWIN_CORE) << "Failed to open" << fileName << "device";
        return -1;
    }

    m_devices[fileDescriptor] = deviceId;
    return fileDescriptor;
}

void SeatdSession::closeRestricted(int fileDescriptor)
{
    auto it = m_devices.find(fileDescriptor);
    if (it == m_devices.end()) {
        return;
    }

    const int deviceId = *it;
    m_devices.erase(it);

    libseat_close_device(m_seat, deviceId);
    close(fileDescriptor);
}

void SeatdSession::switchTo(uint terminal)
{
    libseat_switch_session(m_seat, terminal);
}

void SeatdSession::dispatchEvents()
{
    if (libseat_dispatch(m_seat, 0) == -1) {
        qCCritical(KWIN_CORE) << "Failed to dispatch libseat events";
    }
}

void SeatdSession::updateActive(bool active)
{
    if (m_isActive != active) {
        m_isActive = active;
        emit activeChanged(active);
    }
}

void SeatdSession::handleEnableSeat(libseat *seat, void *userData)
{
    SeatdSession *session = static_cast<SeatdSession *>(userData);
    session->updateActive(true);
}

void SeatdSession::handleDisableSeat(libseat *seat, void *userData)
{
    SeatdSession *session = static_cast<SeatdSession *>(userData);
    session->updateActive(false);

    libseat_disable_seat(seat);
}

} // namespace KWin
