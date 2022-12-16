/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

namespace KWin
{
namespace TabBox
{
class TabBoxSettings;
class SwitchEffectSettings;
class PluginsSettings;

class KWinTabboxData : public KCModuleData
{
    Q_OBJECT

public:
    explicit KWinTabboxData(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    TabBoxSettings *tabBoxConfig() const;
    TabBoxSettings *tabBoxAlternativeConfig() const;
    PluginsSettings *pluginsConfig() const;

private:
    TabBoxSettings *m_tabBoxConfig;
    TabBoxSettings *m_tabBoxAlternativeConfig;
    PluginsSettings *m_pluginsConfig;
};

}

}
