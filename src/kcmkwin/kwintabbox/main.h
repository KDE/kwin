/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tabboxconfig.h"
#include <kcmodule.h>
#include <ksharedconfig.h>

namespace KWin
{
class KWinTabBoxConfigForm;
namespace TabBox
{
class KWinTabboxData;
class TabBoxSettings;
}

class KWinTabBoxConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinTabBoxConfig(QWidget *parent, const QVariantList &args);
    ~KWinTabBoxConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private Q_SLOTS:
    void updateUnmanagedState();
    void updateDefaultIndicator();
    void configureEffectClicked();

private:
    void updateUiFromConfig(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void updateConfigFromUi(const KWinTabBoxConfigForm *form, TabBox::TabBoxSettings *config);
    void updateUiFromDefaultConfig(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void initLayoutLists();
    void setEnabledUi(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void createConnections(KWinTabBoxConfigForm *form);
    bool updateUnmanagedIsNeedSave(const KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    bool updateUnmanagedIsDefault(KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);
    void updateUiDefaultIndicator(bool visible, KWinTabBoxConfigForm *form, const TabBox::TabBoxSettings *config);

private:
    KWinTabBoxConfigForm *m_primaryTabBoxUi = nullptr;
    KWinTabBoxConfigForm *m_alternativeTabBoxUi = nullptr;
    KSharedConfigPtr m_config;

    TabBox::KWinTabboxData *m_data;
};

} // namespace
