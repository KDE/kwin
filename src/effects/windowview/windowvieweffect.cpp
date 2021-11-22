/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowvieweffect.h"
#include "windowview1adaptor.h"

#include <QQuickItem>
#include <QTimer>

namespace KWin
{

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.Effect.WindowView1");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/Effect/WindowView1");

WindowViewEffect::WindowViewEffect()
    : m_shutdownTimer(new QTimer(this))
{
    new WindowView1Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &WindowViewEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &WindowViewEffect::realDeactivate);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/windowview/qml/main.qml"))));
}

WindowViewEffect::~WindowViewEffect()
{
    QDBusConnection::sessionBus().unregisterService(s_dbusServiceName);
    QDBusConnection::sessionBus().unregisterObject(s_dbusObjectPath);
}

QVariantMap WindowViewEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap {
        { QStringLiteral("effect"), QVariant::fromValue(this) },
        { QStringLiteral("targetScreen"), QVariant::fromValue(screen) },
        { QStringLiteral("selectedIds"), QVariant::fromValue(m_windowIds) },
    };
}

int WindowViewEffect::requestedEffectChainPosition() const
{
    return 70;
}

void WindowViewEffect::activate(const QStringList &windowIds)
{
    QList<QUuid> internalIds;
    internalIds.reserve(windowIds.count());
    for (const QString &windowId : windowIds) {
        if (const auto window = effects->findWindow(windowId)) {
            internalIds.append(window->internalId());
            continue;
        }

        // On X11, the task manager can pass a list with X11 ids.
        bool ok;
        if (const long legacyId = windowId.toLong(&ok); ok) {
            if (const auto window = effects->findWindow(legacyId)) {
                internalIds.append(window->internalId());
            }
        }
    }
    if (!internalIds.isEmpty()) {
        m_windowIds = internalIds;
        setRunning(true);
    }
}

void WindowViewEffect::deactivate(int timeout)
{
    const auto screenViews = views();
    for (QuickSceneView *view : screenViews) {
        QMetaObject::invokeMethod(view->rootItem(), "stop");
    }
    m_shutdownTimer->start(timeout);
}

void WindowViewEffect::realDeactivate()
{
    setRunning(false);
}

} // namespace KWin
