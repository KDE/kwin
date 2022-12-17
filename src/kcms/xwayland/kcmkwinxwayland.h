/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickAddons/ManagedConfigModule>
#include <KService>
#include <QAbstractListModel>

#include <kwinxwaylandsettings.h>

class KWinXwaylandData;

class KcmXwayland : public KQuickAddons::ManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KWinXwaylandSettings *settings READ settings CONSTANT)

public:
    explicit KcmXwayland(QObject *parent = nullptr, const QVariantList &list = QVariantList());
    ~KcmXwayland() override;

    KWinXwaylandSettings *settings() const
    {
        return m_settings;
    }

private:
    void refresh();

    KWinXwaylandData *const m_data;
    KWinXwaylandSettings *const m_settings;
};
