/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "outputlocator.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "effect/offscreenquickview.h"

#include <algorithm>

#include <KLocalizedString>

#include <QDBusConnection>
#include <QQuickItem>

namespace KWin
{

static QString outputName(const Output *screen)
{
    const auto screens = effects->screens();
    const bool shouldShowSerialNumber = std::any_of(screens.cbegin(), screens.cend(), [screen](const Output *other) {
        return other != screen && other->manufacturer() == screen->manufacturer() && other->model() == screen->model();
    });
    const bool shouldShowConnector = shouldShowSerialNumber && std::any_of(screens.cbegin(), screens.cend(), [screen](const Output *other) {
        return other != screen && other->serialNumber() == screen->serialNumber();
    });

    QStringList parts;
    if (!screen->manufacturer().isEmpty()) {
        parts.append(screen->manufacturer());
    }

    if (!screen->model().isEmpty()) {
        parts.append(screen->model());
    }

    if (shouldShowSerialNumber && !screen->serialNumber().isEmpty()) {
        parts.append(screen->serialNumber());
    }

    if (shouldShowConnector) {
        parts.append(screen->name());
    }

    if (parts.isEmpty()) {
        return i18nc("@label", "Unknown");
    } else {
        return parts.join(QLatin1Char(' '));
    }
}

OutputLocatorEffect::OutputLocatorEffect(QObject *parent)
    : Effect(parent)
    , m_qmlUrl(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QLatin1String("/effects/outputlocator/qml/OutputLabel.qml"))))
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/Effect/OutputLocator1"),
                                                 QStringLiteral("org.kde.KWin.Effect.OutputLocator1"),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);
    connect(&m_showTimer, &QTimer::timeout, this, &OutputLocatorEffect::hide);
}

bool OutputLocatorEffect::isActive() const
{
    return m_showTimer.isActive();
}

void OutputLocatorEffect::show()
{
    if (isActive()) {
        m_showTimer.start(std::chrono::milliseconds(2500));
        return;
    }

    const auto screens = effects->screens();
    for (const auto screen : screens) {
        auto scene = new OffscreenQuickScene();
        scene->setSource(m_qmlUrl, {{QStringLiteral("outputName"), outputName(screen)}, {QStringLiteral("resolution"), screen->pixelSize()}, {QStringLiteral("scale"), screen->scale()}});
        QRectF geometry(0, 0, scene->rootItem()->implicitWidth(), scene->rootItem()->implicitHeight());
        geometry.moveCenter(screen->geometry().center());
        scene->setGeometry(geometry.toRect());
        connect(scene, &OffscreenQuickView::repaintNeeded, this, [scene] {
            effects->addRepaint(scene->geometry());
        });
        m_scenesByScreens[screen].reset(scene);
    }

    m_showTimer.start(std::chrono::milliseconds(2500));
}

void OutputLocatorEffect::hide()
{
    m_showTimer.stop();

    QRegion repaintRegion;
    for (const auto &[screen, scene] : m_scenesByScreens) {
        repaintRegion += scene->geometry();
    }

    m_scenesByScreens.clear();
    effects->addRepaint(repaintRegion);
}

void OutputLocatorEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);
    // On X11 all screens are painted at once
    if (effects->waylandDisplay()) {
        if (auto it = m_scenesByScreens.find(screen); it != m_scenesByScreens.end()) {
            effects->renderOffscreenQuickView(renderTarget, viewport, it->second.get());
        }
    } else {
        for (const auto &[screen, scene] : m_scenesByScreens) {
            effects->renderOffscreenQuickView(renderTarget, viewport, scene.get());
        }
    }
}
}

#include "moc_outputlocator.cpp"
