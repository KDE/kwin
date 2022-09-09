/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "outputlocator.h"

#include <algorithm>
#include <kwinoffscreenquickview.h>

#include <KLocalizedString>

#include <QDBusConnection>
#include <QQuickItem>

namespace KWin
{

static QString outputName(const EffectScreen *screen)
{
    const auto screens = effects->screens();
    const bool shouldShowSerialNumber = std::any_of(screens.cbegin(), screens.cend(), [screen](const EffectScreen *other) {
        return other != screen && other->manufacturer() == screen->manufacturer() && other->model() == screen->model();
    });
    const bool shouldShowConnector = shouldShowSerialNumber && std::any_of(screens.cbegin(), screens.cend(), [screen](const EffectScreen *other) {
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
    , m_qmlUrl(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/effects/outputlocator/qml/OutputLabel.qml")))
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

    // Needed until Qt6 https://codereview.qt-project.org/c/qt/qtdeclarative/+/361506
    m_dummyWindow = std::make_unique<QWindow>();
    m_dummyWindow->create();

    const auto screens = effects->screens();
    for (const auto screen : screens) {
        auto scene = new OffscreenQuickScene(this, m_dummyWindow.get());
        scene->setSource(m_qmlUrl, {{QStringLiteral("outputName"), outputName(screen)}, {QStringLiteral("resolution"), screen->geometry().size()}, {QStringLiteral("scale"), screen->devicePixelRatio()}});
        QRectF geometry(0, 0, scene->rootItem()->implicitWidth(), scene->rootItem()->implicitHeight());
        geometry.moveCenter(screen->geometry().center());
        scene->setGeometry(geometry.toRect());
        connect(scene, &OffscreenQuickView::repaintNeeded, this, [scene] {
            effects->addRepaint(scene->geometry());
        });
        m_scenesByScreens.insert(screen, scene);
    }

    m_showTimer.start(std::chrono::milliseconds(2500));
}

void OutputLocatorEffect::hide()
{
    m_showTimer.stop();
    const QRegion repaintRegion = std::accumulate(m_scenesByScreens.cbegin(), m_scenesByScreens.cend(), QRegion(), [](QRegion region, OffscreenQuickScene *scene) {
        return region |= scene->geometry();
    });
    qDeleteAll(m_scenesByScreens);
    m_scenesByScreens.clear();
    effects->addRepaint(repaintRegion);
}

void OutputLocatorEffect::paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
    // On X11 all screens are painted at once
    if (effects->waylandDisplay()) {
        if (auto scene = m_scenesByScreens.value(data.screen())) {
            effects->renderOffscreenQuickView(scene);
        }
    } else {
        for (auto scene : m_scenesByScreens) {
            effects->renderOffscreenQuickView(scene);
        }
    }
}
}
