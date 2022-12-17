/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QVector>

#include <KCModuleData>
#include <KPluginMetaData>
#include <KSharedConfig>

class KWinScriptsData : public KCModuleData
{
    Q_OBJECT

public:
    KWinScriptsData(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    bool isDefaults() const override;

    QVector<KPluginMetaData> pluginMetaDataList() const;

private:
    KSharedConfigPtr m_kwinConfig;
};
