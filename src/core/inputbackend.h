/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <KSharedConfig>

#include <QObject>

namespace KWin
{

class InputDevice;

class KWIN_EXPORT InputBackend : public QObject
{
    Q_OBJECT

public:
    explicit InputBackend(QObject *parent = nullptr);

    KSharedConfigPtr config() const;
    void setConfig(KSharedConfigPtr config);

    virtual void initialize()
    {
    }

    virtual void updateScreens()
    {
    }

Q_SIGNALS:
    void deviceAdded(InputDevice *device);
    void deviceRemoved(InputDevice *device);

private:
    KSharedConfigPtr m_config;
};

} // namespace KWin
