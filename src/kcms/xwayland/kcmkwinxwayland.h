/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickManagedConfigModule>
#include <QAbstractListModel>

#include <kscreenglobalsettings.h>
#include <kwinxwaylandsettings.h>

class KWinXwaylandData;

class KcmXwayland : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KWinXwaylandSettings *settings READ settings CONSTANT)
    Q_PROPERTY(KScreenGlobalSettings *kscreenSettings READ kscreenSettings CONSTANT)

public:
    explicit KcmXwayland(QObject *parent, const KPluginMetaData &metaData);
    ~KcmXwayland() override;

    KWinXwaylandSettings *settings() const
    {
        return m_settings;
    }

    KScreenGlobalSettings *kscreenSettings() const
    {
        return m_kscreenSettings;
    }

    Q_INVOKABLE void logout() const;

Q_SIGNALS:
    void showLogoutMessage();

private:
    void refresh();

    KWinXwaylandData *const m_data;
    KWinXwaylandSettings *const m_settings;
    KScreenGlobalSettings *const m_kscreenSettings;
};
