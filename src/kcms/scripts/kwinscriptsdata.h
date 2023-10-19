/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QObject>

#include <KCModuleData>
#include <KPluginMetaData>
#include <KSharedConfig>

class KWinScriptsData : public KCModuleData
{
    Q_OBJECT

public:
    KWinScriptsData(QObject *parent);

    bool isDefaults() const override;

    QList<KPluginMetaData> pluginMetaDataList() const;

private:
    KSharedConfigPtr m_kwinConfig;
};
