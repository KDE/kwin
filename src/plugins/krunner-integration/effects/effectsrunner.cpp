/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effectsrunner.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusPendingCall>

K_PLUGIN_CLASS_WITH_JSON(EffectsRunner, "effectsrunner.json")

EffectsRunner::EffectsRunner(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
    : Plasma::AbstractRunner(parent, metaData, args)
{
    setObjectName(QStringLiteral("Window Management Effects"));

    addSyntax(Plasma::RunnerSyntax(
        i18nc("Note this is a KRunner keyword", "overview"),
        i18n("Allows you to overview virtual desktops and windows")));
    addSyntax(Plasma::RunnerSyntax(
        i18nc("Note this is a KRunner keyword", "present windows"),
        i18n("Zoom out until all opened windows can be displayed side-by-side")));
    addSyntax(Plasma::RunnerSyntax(
        i18nc("Note this is a KRunner keyword", "desktop grid"),
        i18n("Zoom out so all desktops are displayed side by side in a grid")));
}

EffectsRunner::~EffectsRunner()
{
}

void EffectsRunner::match(Plasma::RunnerContext &c)
{
    const QString term = c.query();

    if (matchesKeyword(term, "overview")) {
        addEffectMatch(c, OverviewAction);
    } else if (matchesKeyword(term, "present windows")) {
        addEffectMatch(c, PresentWindowsAllDesktopsAction);
        addEffectMatch(c, PresentWindowsCurrentDesktopAction);
        addEffectMatch(c, PresentWindowsWindowClassAllDesktopsAction);
        addEffectMatch(c, PresentWindowsWindowClassCurrentDesktopAction);
    } else if (matchesKeyword(term, "desktop grid")) {
        addEffectMatch(c, DesktopGridAction);
    } else if (matchesKeyword(term, "present")) {
        addEffectMatch(c, PresentWindowsAllDesktopsAction, false);
        addEffectMatch(c, PresentWindowsCurrentDesktopAction, false);
        addEffectMatch(c, PresentWindowsWindowClassAllDesktopsAction, false);
        addEffectMatch(c, PresentWindowsWindowClassCurrentDesktopAction, false);
    } else if (matchesKeyword(term, "grid")) {
        addEffectMatch(c, DesktopGridAction, false);
    } else if (matchesKeyword(term, "windows") || matchesKeyword(term, "window")) {
        addEffectMatch(c, OverviewAction, false);
        addEffectMatch(c, PresentWindowsAllDesktopsAction, false);
        addEffectMatch(c, PresentWindowsCurrentDesktopAction, false);
        addEffectMatch(c, PresentWindowsWindowClassAllDesktopsAction, false);
        addEffectMatch(c, PresentWindowsWindowClassCurrentDesktopAction, false);
    } else if (matchesKeyword(term, "desktops") || matchesKeyword(term, "desktop")) {
        addEffectMatch(c, OverviewAction, false);
        addEffectMatch(c, DesktopGridAction, false);
    }
}

bool EffectsRunner::matchesKeyword(const QString &term, const char *keyword)
{
    return term.compare(i18nc("Note this is a KRunner keyword", keyword), Qt::CaseInsensitive) == 0;
}

void EffectsRunner::addEffectMatch(Plasma::RunnerContext &c, EffectAction action, bool exactMatch)
{
    KConfigGroup pluginConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::SimpleConfig)->group("Plugins");
    const bool overviewEnabled = pluginConfig.readEntry("overviewEnabled", true);
    const bool presentWindowsEnabled = pluginConfig.readEntry("windowviewEnabled", true);
    const bool desktopGridEnabled = pluginConfig.readEntry("desktopgridEnabled", true);

    Plasma::QueryMatch match(this);

    switch (action) {
    case OverviewAction:
        if (!overviewEnabled) {
            return;
        }
        match.setText(i18n("Overview"));
        match.setSubtext(i18n("Overview virtual desktops and windows"));
        break;
    case PresentWindowsAllDesktopsAction:
        if (!presentWindowsEnabled) {
            return;
        }
        match.setText(i18n("Present Windows: All desktops"));
        match.setSubtext(i18n("Overview windows on all virtual desktops"));
        break;
    case PresentWindowsCurrentDesktopAction:
        if (!presentWindowsEnabled) {
            return;
        }
        match.setText(i18n("Present Windows: Current desktop"));
        match.setSubtext(i18n("Overview windows on the current virtual desktop"));
        break;
    case PresentWindowsWindowClassAllDesktopsAction:
        if (!presentWindowsEnabled) {
            return;
        }
        match.setText(i18n("Present Windows: Current application on all desktops"));
        match.setSubtext(i18n("Overview windows of the current application on all desktops"));
        break;
    case PresentWindowsWindowClassCurrentDesktopAction:
        if (!presentWindowsEnabled) {
            return;
        }
        match.setText(i18n("Present Windows: Current application on current desktop"));
        match.setSubtext(i18n("Overview windows of the current application on the current desktop"));
        break;
    case DesktopGridAction:
        if (!desktopGridEnabled) {
            return;
        }
        match.setText(i18n("Desktop Grid"));
        match.setSubtext(i18n("Overview virtual desktops"));
        break;
    }
    match.setData(action);
    match.setType(exactMatch ? Plasma::QueryMatch::ExactMatch : Plasma::QueryMatch::PossibleMatch);
    match.setRelevance(exactMatch ? 1.0 : 0.8);
    match.setIconName(QStringLiteral("preferences-desktop-effects"));
    match.setMatchCategory("Desktop Effects");

    c.addMatch(match);
}

void EffectsRunner::run(const Plasma::RunnerContext &c, const Plasma::QueryMatch &match)
{
    EffectAction action = EffectAction(match.data().toInt());
    switch (action) {
    case OverviewAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/Overview1"),
            QStringLiteral("org.kde.KWin.Effect.Overview1"),
            QStringLiteral("activate")));
        break;
    case PresentWindowsAllDesktopsAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/WindowView1"),
            QStringLiteral("org.kde.KWin.Effect.WindowView1"),
            QStringLiteral("activateAllDesktops")));
        break;
    case PresentWindowsCurrentDesktopAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/WindowView1"),
            QStringLiteral("org.kde.KWin.Effect.WindowView1"),
            QStringLiteral("activateCurrentDesktop")));
        break;
    case PresentWindowsWindowClassAllDesktopsAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/WindowView1"),
            QStringLiteral("org.kde.KWin.Effect.WindowView1"),
            QStringLiteral("activateWindowClassAllDesktops")));
        break;
    case PresentWindowsWindowClassCurrentDesktopAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/WindowView1"),
            QStringLiteral("org.kde.KWin.Effect.WindowView1"),
            QStringLiteral("activateWindowClassCurrentDesktop")));
        break;
    case DesktopGridAction:
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/org/kde/KWin/Effect/DesktopGrid1"),
            QStringLiteral("org.kde.KWin.Effect.DesktopGrid1"),
            QStringLiteral("activate")));
        break;
    }
}

#include "effectsrunner.moc"
