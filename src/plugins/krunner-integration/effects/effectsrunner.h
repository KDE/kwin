/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <krunner/abstractrunner.h>

class EffectsRunner : public Plasma::AbstractRunner
{
    Q_OBJECT

public:
    EffectsRunner(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args);
    ~EffectsRunner() override;

    void match(Plasma::RunnerContext &context) override;
    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match) override;

private:
    enum EffectAction {
        OverviewAction,
        PresentWindowsAllDesktopsAction,
        PresentWindowsCurrentDesktopAction,
        PresentWindowsWindowClassAllDesktopsAction,
        PresentWindowsWindowClassCurrentDesktopAction,
        DesktopGridAction
    };
    void addEffectMatch(Plasma::RunnerContext &context, EffectAction action, bool exactMatch = true);
    bool matchesKeyword(const QString &term, const char *keyword);
};
