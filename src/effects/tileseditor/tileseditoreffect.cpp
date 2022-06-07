/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tileseditoreffect.h"
#include "tileseditorconfig.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.Effect.WindowView1");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/Effect/WindowView1");

TilesEditorEffect::TilesEditorEffect()
    : m_shutdownTimer(new QTimer(this))
{
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &TilesEditorEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &TilesEditorEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_T;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &TilesEditorEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Edit Tiles"));
    m_toggleAction->setText(i18n("Toggle Tiles Editor"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut({defaultToggleShortcut}, m_toggleAction);

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
    TilesEditorConfig::self()->read();
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
    const auto screenViews = views();
    for (QuickSceneView *view : screenViews) {
        QMetaObject::invokeMethod(view->rootItem(), "stop");
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
