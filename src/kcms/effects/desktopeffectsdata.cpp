/*
    SPDX-FileCopyrightText: 2021 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopeffectsdata.h"
#include "effectsmodel.h"

namespace KWin
{

DesktopEffectsData::DesktopEffectsData(QObject *parent)
    : KCModuleData(parent)
    , m_model(new EffectsModel(this))

{
    disconnect(this, &KCModuleData::aboutToLoad, nullptr, nullptr);
    connect(m_model, &EffectsModel::loaded, this, &KCModuleData::loaded);

    // These are handled in kcm_animations
    m_model->setExcludeExclusiveGroups({"toplevel-open-close-animation", "maximize", "minimize", "fullscreen", "show-desktop", "desktop-animations"});
    m_model->setExcludeEffects({"fadingpopups", "slidingpopups", "login", "logout"});

    m_model->load();
}

DesktopEffectsData::~DesktopEffectsData()
{
}

bool DesktopEffectsData::isDefaults() const
{
    return m_model->isDefaults();
}

}

#include "moc_desktopeffectsdata.cpp"
