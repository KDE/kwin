/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/inputdevice.h"

#include <kglobalaccel_interface.h>

#include <QObject>

class KGlobalAccelImpl : public KGlobalAccelInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KGlobalAccelInterface_iid FILE "kwin.json")
    Q_INTERFACES(KGlobalAccelInterface)

public:
    KGlobalAccelImpl(QObject *parent = nullptr);
    ~KGlobalAccelImpl() override;

    bool grabKey(int key, bool grab) override;
    bool setTriggerActive(const KGlobalShortcutTrigger &trigger,
                          bool active,
                          const QString &componentName,
                          const QString &actionId,
                          const QString &componentFriendlyName,
                          const QString &actionFriendlyName) override;

Q_SIGNALS:
    void triggerActive(const KGlobalShortcutTrigger &trigger,
                       bool active,
                       const QString &componentName,
                       const QString &actionId,
                       const QString &componentFriendlyName,
                       const QString &actionFriendlyName);

public Q_SLOTS:
    bool checkKeyPressed(int keyQt, KWin::KeyboardKeyState state);
    bool checkPointerPressed(Qt::MouseButtons buttons);
    bool checkAxisTriggered(int axis);
    bool checkTriggerEvent(const KGlobalShortcutTrigger &trigger, ShortcutTriggerEvent event);
    void cancelModiferOnlySequence();
};
