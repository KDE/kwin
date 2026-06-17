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

static QString outputName(const LogicalOutput *screen)
{
    const auto screens = effects->screens();
    const bool shouldShowSerialNumber = std::any_of(screens.cbegin(), screens.cend(), [screen](const LogicalOutput *other) {
        return other != screen && other->manufacturer() == screen->manufacturer() && other->model() == screen->model();
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

    parts.append(screen->name());

    return parts.join(QLatin1Char('\n'));
}

OutputLocatorEffect::OutputLocatorEffect(QObject *parent)
    : Effect(parent)
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

    // Sort screens in the same order as KScreen does
    QList<LogicalOutput *> sortedScreens = screens;
    std::sort(sortedScreens.begin(), sortedScreens.end(), [](LogicalOutput *a, LogicalOutput *b) {
        return QString::compare(a->name(), b->name(), Qt::CaseInsensitive) < 0;
    });

    for (const auto screen : screens) {
        const int index = sortedScreens.indexOf(screen);
        const QString number = (index >= 0) ? QString::number(index + 1) : QString();

        auto scene = new OffscreenQuickScene();
        scene->loadFromModule(QStringLiteral("org.kde.kwin.outputlocator"), QStringLiteral("OutputLabel"), {{QStringLiteral("outputName"), outputName(screen)}, {QStringLiteral("number"), number},{QStringLiteral("resolution"), screen->pixelSize()}, {QStringLiteral("scale"), screen->scale()}});
        RectF geometry(0, 0, scene->rootItem()->implicitWidth(), scene->rootItem()->implicitHeight());
        geometry.moveCenter(screen->geometry().center());
        scene->setGeometry(geometry.toRect());
        m_scenesByScreens[screen].reset(scene);
    }

    m_showTimer.start(std::chrono::milliseconds(2500));
}

void OutputLocatorEffect::hide()
{
    m_showTimer.stop();

    Region repaintRegion;
    for (const auto &[screen, scene] : m_scenesByScreens) {
        repaintRegion += scene->geometry();
    }

    m_scenesByScreens.clear();
    effects->addRepaint(repaintRegion);
}

}

#include "moc_outputlocator.cpp"
