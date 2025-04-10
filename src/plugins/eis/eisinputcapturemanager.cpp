/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisinputcapturemanager.h"

#include "eisinputcapture.h"
#include "eisinputcapturefilter.h"
#include "inputcapture_logging.h"

#include "core/output.h"
#include "cursor.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "workspace.h"
#include "xkb.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>

#include <libeis.h>

namespace KWin
{

constexpr QKeyCombination defaultDisableKeys{Qt::META | Qt::SHIFT, Qt::Key_Escape};

class BarrierSpy : public InputEventSpy
{
public:
    BarrierSpy(EisInputCaptureManager *manager)
        : manager(manager)
    {
    }
    void pointerMotion(KWin::PointerMotionEvent *event) override
    {
        if (manager->activeCapture()) {
            return;
        }
        for (const auto &capture : manager->m_inputCaptures) {
            for (const auto &barrier : capture->barriers()) {
                // Detect the user trying to move out of the workArea and across the barrier:
                // Both current and previous positions are on the barrier but there was an orthogonal delta
                if (barrier.hitTest(event->position) && barrier.hitTest(previousPos) && ((barrier.orientation == Qt::Vertical && event->delta.x() != 0) || (barrier.orientation == Qt::Horizontal && event->delta.y() != 0))) {
                    qCDebug(KWIN_INPUTCAPTURE) << "Activating input capture, crossing"
                                               << "barrier(" << barrier.orientation << barrier.position << "[" << barrier.start << "," << barrier.end << "])"
                                               << "at" << event->position << "with" << event->delta;
                    manager->barrierHit(capture.get(), event->position + event->delta);
                    break;
                }
            }
        }
        previousPos = event->position;
    }
    void keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        if (!manager->activeCapture()) {
            return;
        }
        if (event->state != KeyboardKeyState::Pressed) {
            return;
        }
        // Even if the user removed all sequences for this, we use the default one to have an escape hatch
        auto disableKeySequence = KGlobalAccel::self()->shortcut(manager->m_disableCaptureAction).value(0, defaultDisableKeys)[0];
        if (event->key == disableKeySequence.key() && event->modifiers == disableKeySequence.keyboardModifiers()) {
            manager->activeCapture()->disable();
        }
    }

private:
    QKeyCombination currentCombination;
    EisInputCaptureManager *manager;
    QPointF previousPos;
};

bool EisInputCaptureBarrier::hitTest(const QPointF &point) const
{
    if (orientation == Qt::Vertical) {
        return point.x() == position && start <= point.y() && point.y() <= end;
    }
    return point.y() == position && start <= point.x() && point.x() <= end;
}

EisInputCaptureManager::EisInputCaptureManager()
    : m_serviceWatcher(new QDBusServiceWatcher(this))
    , m_barrierSpy(std::make_unique<BarrierSpy>(this))
    , m_inputFilter(std::make_unique<EisInputCaptureFilter>(this))
{
    qDBusRegisterMetaType<QPair<QPoint, QPoint>>();
    qDBusRegisterMetaType<QList<QPair<QPoint, QPoint>>>();

    const auto keymap = input()->keyboard()->xkb()->keymapContents();
    if (!keymap.isEmpty()) {
        m_keymapFile = RamFile("input capture keymap", keymap.data(), keymap.size(), RamFile::Flag::SealWrite);
    }
    connect(input()->keyboard()->keyboardLayout(), &KeyboardLayout::layoutChanged, this, [this] {
        const auto keymap = input()->keyboard()->xkb()->keymapContents();
        if (!keymap.isEmpty()) {
            m_keymapFile = RamFile("input capture keymap", keymap.data(), keymap.size(), RamFile::Flag::SealWrite);
        } else {
            m_keymapFile = RamFile();
        }
    });

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &service) {
        if (m_activeCapture && m_activeCapture->dbusService == service) {
            deactivate();
        }
        std::erase_if(m_inputCaptures, [&service](const std::unique_ptr<EisInputCapture> &capture) {
            return capture->dbusService == service;
        });
        m_serviceWatcher->removeWatchedService(service);
    });

    m_disableCaptureAction = new QAction(this);
    m_disableCaptureAction->setProperty("componentName", QStringLiteral("kwin"));
    m_disableCaptureAction->setObjectName(QStringLiteral("disableInputCapture"));
    m_disableCaptureAction->setText(i18nc("@action shortcut", "Disable Active Input Capture"));
    KGlobalAccel::setGlobalShortcut(m_disableCaptureAction, QKeySequence(defaultDisableKeys));

    QDBusConnection::sessionBus().registerObject("/org/kde/KWin/EIS/InputCapture", "org.kde.KWin.EIS.InputCaptureManager", this, QDBusConnection::ExportAllInvokables | QDBusConnection::ExportAllSignals);
}

EisInputCaptureManager::~EisInputCaptureManager()
{
    if (input()) {
        input()->uninstallInputEventFilter(m_inputFilter.get());
        input()->uninstallInputEventSpy(m_barrierSpy.get());
    }
}

const RamFile &EisInputCaptureManager::keyMap() const
{
    return m_keymapFile;
}

void EisInputCaptureManager::removeInputCapture(const QDBusObjectPath &capture)
{
    auto it = std::ranges::find(m_inputCaptures, capture.path(), &EisInputCapture::dbusPath);
    if (it == std::ranges::end(m_inputCaptures)) {
        return;
    }
    if (m_activeCapture == it->get()) {
        deactivate();
    }
    m_inputCaptures.erase(it);
    if (m_inputCaptures.empty()) {
        input()->uninstallInputEventSpy(m_barrierSpy.get());
    }
}

QDBusObjectPath EisInputCaptureManager::addInputCapture(uint capabilities)
{
    constexpr uint keyboardPortal = 1;
    constexpr uint pointerPortal = 2;
    constexpr uint touchPortal = 4;
    QFlags<eis_device_capability> eisCapabilities;
    if (capabilities & keyboardPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_KEYBOARD;
    }
    if (capabilities & pointerPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_POINTER;
        eisCapabilities |= EIS_DEVICE_CAP_POINTER_ABSOLUTE;
        eisCapabilities |= EIS_DEVICE_CAP_BUTTON;
        eisCapabilities |= EIS_DEVICE_CAP_SCROLL;
    }
    if (capabilities & touchPortal) {
        eisCapabilities |= EIS_DEVICE_CAP_TOUCH;
    }

    const QString dbusService = message().service();
    m_serviceWatcher->addWatchedService(dbusService);

    if (m_inputCaptures.empty()) {
        input()->installInputEventSpy(m_barrierSpy.get());
    }
    auto &capture = m_inputCaptures.emplace_back(std::make_unique<EisInputCapture>(this, dbusService, eisCapabilities));
    connect(capture.get(), &EisInputCapture::deactivated, this, [this] {
        deactivate();
    });
    return QDBusObjectPath(capture->dbusPath());
}

void EisInputCaptureManager::barrierHit(KWin::EisInputCapture *capture, const QPointF &position)
{
    if (m_activeCapture) {
        return;
    }
    m_activeCapture = capture;
    capture->activate(position);
    input()->installInputEventFilter(m_inputFilter.get());
    // Even though the input events are filtered out the cursor is updated on screen which looks weird
    Cursors::self()->hideCursor();
}

void EisInputCaptureManager::deactivate()
{
    m_activeCapture = nullptr;
    m_inputFilter->clearTouches();
    input()->uninstallInputEventFilter(m_inputFilter.get());
    Cursors::self()->showCursor();
}

EisInputCapture *EisInputCaptureManager::activeCapture()
{
    return m_activeCapture;
}

}
