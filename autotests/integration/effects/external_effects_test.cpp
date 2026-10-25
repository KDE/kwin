/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Alexander Berg <wehrwolfmann@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "compositor.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "wayland_server.h"

#include <KConfigGroup>

using namespace KWin;

class ExternalEffectsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testLoadedAtStartup();
};

void ExternalEffectsTest::initTestCase()
{
#if KWIN_TEST_RESTART_COUNT
    qputenv("KWIN_RESTART_COUNT", QByteArray::number(KWIN_TEST_RESTART_COUNT));
#else
    qunsetenv("KWIN_RESTART_COUNT");
#endif
    qRegisterMetaType<KWin::Effect *>();
    QVERIFY(waylandServer()->init(qAppName()));

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto effectNames = EffectLoader().listOfKnownEffects();
    QVERIFY(effectNames.contains(QStringLiteral("externaleffect")));
    for (const QString &name : effectNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    plugins.writeEntry(QStringLiteral("externaleffectEnabled"), true);
    plugins.writeEntry(QStringLiteral("highlightwindowEnabled"), true);
    config->sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(Compositor::self());
}

void ExternalEffectsTest::testLoadedAtStartup()
{
    // built-in effects are always loaded
    QVERIFY(effects->isEffectLoaded(QStringLiteral("highlightwindow")));

#if KWIN_TEST_RESTART_COUNT >= 2
    // kwin keeps crashing, so external effects are not loaded automatically...
    QVERIFY(!effects->isEffectLoaded(QStringLiteral("externaleffect")));

    // ...but the user can still load them
    QVERIFY(effects->loadEffect(QStringLiteral("externaleffect")));
#endif
    QVERIFY(effects->isEffectLoaded(QStringLiteral("externaleffect")));
}

WAYLANDTEST_MAIN(ExternalEffectsTest)
#include "external_effects_test.moc"
