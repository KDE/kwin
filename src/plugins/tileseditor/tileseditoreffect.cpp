/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tileseditoreffect.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

TilesEditorEffect::TilesEditorEffect()
    : m_shutdownTimer(std::make_unique<QTimer>())
{
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer.get(), &QTimer::timeout, this, &TilesEditorEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &TilesEditorEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_T;
    m_toggleAction = std::make_unique<QAction>();
    connect(m_toggleAction.get(), &QAction::triggered, this, &TilesEditorEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Edit Tiles"));
    m_toggleAction->setText(i18n("Toggle Tiles Editor"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction.get(), {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction.get(), {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction.get());

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/tileseditor/qml/main.qml"))));
}

TilesEditorEffect::~TilesEditorEffect()
{
}

QVariantMap TilesEditorEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void TilesEditorEffect::reconfigure(ReconfigureFlags)
{
    setAnimationDuration(animationTime(200));
}

void TilesEditorEffect::toggle()
{
    if (!isRunning()) {
        activate();
    } else {
        deactivate(0);
    }
}

void TilesEditorEffect::activate()
{
    setRunning(true);
}

void TilesEditorEffect::deactivate(int timeout)
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }

    m_shutdownTimer->start(timeout);
}

void TilesEditorEffect::realDeactivate()
{
    setRunning(false);
}

int TilesEditorEffect::animationDuration() const
{
    return m_animationDuration;
}

void TilesEditorEffect::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

int TilesEditorEffect::requestedEffectChainPosition() const
{
    return 70;
}

void TilesEditorEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
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
