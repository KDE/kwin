/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>
#include <ksharedconfig.h>
#include "tabboxconfig.h"

namespace KWin
{
class KWinTabBoxConfigForm;
enum class BuiltInEffect;
namespace TabBox
{
class TabBoxSettings;
class SwitchEffectSettings;
class PluginsSettings;
}


class KWinTabBoxConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinTabBoxConfig(QWidget* parent, const QVariantList& args);
    ~KWinTabBoxConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private Q_SLOTS:
    void updateUnmanagedState();
    void configureEffectClicked();
    void slotGHNS();

private:
    void updateUiFromConfig(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void updateConfigFromUi(const KWinTabBoxConfigForm *form, TabBox::TabBoxSettings *config);
    void updateUiFromDefaultConfig(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void initLayoutLists();
    void setEnabledUi(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void createConnections(KWinTabBoxConfigForm *form);
    bool updateUnmanagedIsNeedSave(const KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    bool updateUnmanagedIsDefault(const KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);

private:
    KWinTabBoxConfigForm *m_primaryTabBoxUi = nullptr;
    KWinTabBoxConfigForm *m_alternativeTabBoxUi = nullptr;
    KSharedConfigPtr m_config;

    TabBox::TabBoxSettings *m_tabBoxConfig;
    TabBox::TabBoxSettings *m_tabBoxAlternativeConfig;
    TabBox::SwitchEffectSettings *m_coverSwitchConfig;
    TabBox::SwitchEffectSettings *m_flipSwitchConfig;
    TabBox::PluginsSettings *m_pluginsConfig;

    // Builtin effects' names
    QString m_coverSwitch;
    QString m_flipSwitch;
};

} // namespace

#endif
