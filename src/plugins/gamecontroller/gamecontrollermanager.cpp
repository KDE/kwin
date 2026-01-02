/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gamecontrollermanager.h"
#include "gamecontroller_logging.h"

#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <QDir>

namespace KWin
{

GameControllerManager::GameControllerManager()
    : m_udev(std::make_unique<Udev>())
    , m_udevMonitor(m_udev->monitor())
{
    if (!m_udevMonitor) {
        qCWarning(KWIN_GAMECONTROLLER) << "Failed to setup udev Monitor:" << strerror(errno);
        return;
    }
    m_udevMonitor->filterSubsystemDevType("input");
    const int udevFd = m_udevMonitor->fd();
    if (udevFd < 0) {
        qCWarning(KWIN_GAMECONTROLLER) << "Failed to setup input system monitor:" << strerror(errno);
        return;
    }

    m_socketNotifier = std::make_unique<QSocketNotifier>(udevFd, QSocketNotifier::Read);
    connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &GameControllerManager::handleUdevEvent);
    m_udevMonitor->enable();

    m_inotifyFd = FileDescriptor(inotify_init1(IN_CLOEXEC | IN_NONBLOCK));
    if (m_inotifyFd.get() < 0) {
        qCWarning(KWIN_GAMECONTROLLER) << "Failed to create inotify instance:" << strerror(errno);
        return;
    }

    m_inotifyNotifier = std::make_unique<QSocketNotifier>(m_inotifyFd.get(), QSocketNotifier::Read);
    connect(m_inotifyNotifier.get(), &QSocketNotifier::activated, this, &GameControllerManager::handleFdAccess);

    const QString inputDir = qEnvironmentVariable("KWIN_TEST_INPUT_DIR", QStringLiteral("/dev/input"));
    const QDir dir(inputDir);
    const auto files = dir.entryList({QStringLiteral("event*")}, QDir::Files | QDir::Readable | QDir::System);
    for (const QString &file : files) {
        const QString path = dir.absoluteFilePath(file);
        if (!isTracked(path)) {
            addGameController(path);
        }
    }
}

GameControllerManager::~GameControllerManager()
{
    for (auto it = m_deviceWatches.begin(); it != m_deviceWatches.end(); ++it) {
        inotify_rm_watch(m_inotifyFd.get(), it.value());
    }
    m_deviceWatches.clear();

    m_watchesToGameControllers.clear();
    m_gameControllers.clear();
}

void GameControllerManager::handleUdevEvent()
{
    while (std::unique_ptr<UdevDevice> device = m_udevMonitor->getDevice()) {
        if (!device->devNode().startsWith(QStringLiteral("/dev/input/event"))) {
            continue;
        }

        if (device->action() == QLatin1StringView("add")) {
            qCDebug(KWIN_GAMECONTROLLER) << "Received unexpected udev add event for input event device:" << device->devNode();
            addGameController(device->devNode());
        } else if (device->action() == QLatin1StringView("remove")) {
            qCDebug(KWIN_GAMECONTROLLER) << "Received unexpected udev remove event for input event device:" << device->devNode();
            removeGameController(device->devNode());
        }
    }
}

GameController *GameControllerManager::addGameController(const QString &devNode)
{
    // Note: Game controllers are generally *not* restricted by udev on most
    // distributions - allows games to open them directly without special
    // privileges. Using ::open() here is intentional.
    // openRestricted() is meant for devices that require elevated access
    // (e.g. DRM, keyboards, mice), it will fail for game controllers and
    // provides unclear error logs.

    FileDescriptor fd(::open(devNode.toUtf8().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC));
    if (fd.get() < 0) {
        qCWarning(KWIN_GAMECONTROLLER, "Failed to open fd for: %s: %s", qPrintable(devNode), strerror(errno));
        return nullptr;
    }
    qCDebug(KWIN_GAMECONTROLLER) << "Opened fd for:" << devNode;

    libevdev *evdev = nullptr;
    const int err = libevdev_new_from_fd(fd.get(), &evdev);
    if (err < 0) {
        qCWarning(KWIN_GAMECONTROLLER, "libevdev_new_from_fd failed for %s: %s", qPrintable(devNode), strerror(-err));
        return nullptr;
    }

    if (!isGameControllerDevice(evdev)) {
        qCDebug(KWIN_GAMECONTROLLER) << "Not a controller (capability check failed):" << devNode;
        libevdev_free(evdev);
        return nullptr;
    }

    std::unique_ptr<GameController> gamecontroller = std::make_unique<GameController>(devNode, std::move(fd), evdev);
    // If watch device added, we can officially "add" controller
    if (addDeviceWatch(gamecontroller.get())) {
        m_gameControllers.push_back(std::move(gamecontroller));

        GameController *controller = m_gameControllers.back().get();
        qCDebug(KWIN_GAMECONTROLLER, "Added Game Controller: %s: %s", qPrintable(libevdev_get_name(controller->evdev())), qPrintable(controller->path()));
        return controller;
    }

    qCWarning(KWIN_GAMECONTROLLER, "Failed to initialize GameController object or initialize watch device for: %s", qPrintable(devNode));
    return nullptr;
}

bool GameControllerManager::removeGameController(const QString &devNode)
{
    for (auto it = m_gameControllers.begin(); it != m_gameControllers.end(); ++it) {
        const GameController *controller = it->get();
        if (controller->path() == devNode) {
            if (m_deviceWatches.contains(devNode)) {
                const int watch = m_deviceWatches[devNode];
                inotify_rm_watch(m_inotifyFd.get(), watch);
                m_deviceWatches.remove(devNode);
                m_watchesToGameControllers.remove(watch);
            }

            m_gameControllers.erase(it);
            qCDebug(KWIN_GAMECONTROLLER) << "GameController removed:" << devNode;
            return true;
        }
    }
    return false;
}

bool GameControllerManager::isGameControllerDevice(libevdev *evdev)
{
    if (!evdev) {
        return false;
    }

    if (libevdev_has_event_type(evdev, EV_ABS) && (libevdev_has_event_code(evdev, EV_ABS, ABS_X) || libevdev_has_event_code(evdev, EV_ABS, ABS_HAT0X))) {
        qCDebug(KWIN_GAMECONTROLLER) << "Has Analog Joysticks or D-pad, likely a Game Controller.";
        return true;
    }

    if (libevdev_has_event_type(evdev, EV_KEY) && (libevdev_has_event_code(evdev, EV_KEY, BTN_JOYSTICK) || libevdev_has_event_code(evdev, EV_KEY, BTN_GAMEPAD))) {
        qCDebug(KWIN_GAMECONTROLLER) << "Has BTN_JOYSTICK/GAMEPAD, likely a Game Controller.";
        return true;
    }

    return false;
}

bool GameControllerManager::isTracked(const QString &path) const
{
    return std::ranges::any_of(m_gameControllers, [path](const auto &controller) {
        return controller->path() == path;
    });
}

bool GameControllerManager::addDeviceWatch(GameController *controller)
{
    if (m_deviceWatches.contains(controller->path())) {
        return false;
    }

    const int watch = inotify_add_watch(m_inotifyFd.get(), controller->path().toUtf8().constData(), IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE);
    if (watch < 0) {
        qCWarning(KWIN_GAMECONTROLLER) << "Failed to add inotify watch for" << controller->path() << ":" << strerror(errno);
        return false;
    }
    m_deviceWatches[controller->path()] = watch;
    m_watchesToGameControllers[watch] = controller;

    qCDebug(KWIN_GAMECONTROLLER) << "Added device watch for" << controller->path();
    return true;
}

void GameControllerManager::handleFdAccess()
{
    char buffer[4096];
    const ssize_t length = read(m_inotifyFd.get(), buffer, sizeof(buffer));

    if (length < 0) {
        if (errno != EAGAIN) {
            qCWarning(KWIN_GAMECONTROLLER) << "Error reading inotify events:" << strerror(errno);
        }
        return;
    }

    for (char *ptr = buffer; ptr < buffer + length;) {
        const struct inotify_event *event = reinterpret_cast<struct inotify_event *>(ptr);

        const auto it = m_watchesToGameControllers.find(event->wd);
        if (it != m_watchesToGameControllers.end()) {
            GameController *controller = it.value();
            if (event->mask & IN_OPEN) {
                controller->countUsage(+1);
            } else if (event->mask & (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)) {
                controller->countUsage(-1);
            }
            qCDebug(KWIN_GAMECONTROLLER) << "Device" << controller->path() << "in use by:" << controller->usageCount() << "other apps";
        }
        ptr += sizeof(struct inotify_event) + event->len;
    }
}

} // namespace KWin

#include "moc_gamecontrollermanager.cpp"
