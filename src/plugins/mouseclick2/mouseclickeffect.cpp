/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclickeffect.h"
#include "effect/effecthandler.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>

#include <KGlobalAccel>
#include <KLocalizedString>

using namespace std::chrono_literals;

namespace KWin
{

MouseClickEffect2::MouseClickEffect2()
    : m_shutdownTimer(std::make_unique<QTimer>())
{
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer.get(), &QTimer::timeout, this, &MouseClickEffect2::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &MouseClickEffect2::realDeactivate);

    m_toggleAction = std::make_unique<QAction>();
    connect(m_toggleAction.get(), &QAction::triggered, this, &MouseClickEffect2::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Mouse Mark 2"));
    m_toggleAction->setText(i18n("Toggle Mouse Mark"));
    m_toggleAction->setAutoRepeat(false);
    KGlobalAccel::self()->setGlobalShortcut(m_toggleAction.get(), QKeySequence(Qt::META | Qt::Key_Y));
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction.get());

    loadFromModule(QStringLiteral("org.kde.kwin/mousemark2"), QStringLiteral("Main"));
}

MouseClickEffect2::~MouseClickEffect2()
{
}
/*
QVariantMap MouseClickEffect2::initialProperties(LogicalOutput *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
    };
}*/

void MouseClickEffect2::reconfigure(ReconfigureFlags)
{
    setAnimationDuration(animationTime(200ms));
}

void MouseClickEffect2::toggle()
{
    if (!isRunning()) {
        activate();
    } else {
        deactivate(0);
    }
}

void MouseClickEffect2::activate()
{
    setRunning(true);
}

void MouseClickEffect2::deactivate(int timeout)
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }

    m_shutdownTimer->start(timeout);
}

void MouseClickEffect2::realDeactivate()
{
    setRunning(false);
}

int MouseClickEffect2::animationDuration() const
{
    return m_animationDuration;
}

void MouseClickEffect2::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

int MouseClickEffect2::requestedEffectChainPosition() const
{
    return 70;
}

void MouseClickEffect2::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

} // namespace KWin

#include "moc_mouseclickeffect.cpp"
