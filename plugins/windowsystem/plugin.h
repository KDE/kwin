/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <KWindowSystem/private/kwindowsystemplugininterface_p.h>

class KWindowSystemKWinPlugin : public KWindowSystemPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kwindowsystem.KWindowSystemPluginInterface" FILE "kwindowsystem.json")
    Q_INTERFACES(KWindowSystemPluginInterface)

public:
    explicit KWindowSystemKWinPlugin(QObject *parent = nullptr);
    ~KWindowSystemKWinPlugin() override;

    KWindowEffectsPrivate *createEffects() override;
    KWindowSystemPrivate *createWindowSystem() override;
    KWindowShadowTilePrivate *createWindowShadowTile() override;
    KWindowShadowPrivate *createWindowShadow() override;
};
