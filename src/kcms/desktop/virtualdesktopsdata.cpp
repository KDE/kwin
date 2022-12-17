/*
    SPDX-FileCopyrightText: 2021 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "virtualdesktopsdata.h"

#include "animationsmodel.h"
#include "desktopsmodel.h"
#include "virtualdesktopssettings.h"

namespace KWin
{

VirtualDesktopsData::VirtualDesktopsData(QObject *parent, const QVariantList &args)
    : KCModuleData(parent, args)
    , m_settings(new VirtualDesktopsSettings(this))
    , m_desktopsModel(new DesktopsModel(this))
    , m_animationsModel(new AnimationsModel(this))
{
    // Default behavior of KCModuleData is to emit loaded signal after being initialized.
    // To handle asynchronous load of EffectsModel we disable default behavior and
    // emit loaded signal when EffectsModel is actually loaded.
    disconnect(this, &KCModuleData::aboutToLoad, nullptr, nullptr);
    connect(m_animationsModel, &EffectsModel::loaded, this, &KCModuleData::loaded);

    m_desktopsModel->load();
    m_animationsModel->load();
}

bool VirtualDesktopsData::isDefaults() const
{
    return m_animationsModel->isDefaults() && m_desktopsModel->isDefaults() && m_settings->isDefaults();
}

VirtualDesktopsSettings *VirtualDesktopsData::settings() const
{
    return m_settings;
}

DesktopsModel *VirtualDesktopsData::desktopsModel() const
{
    return m_desktopsModel;
}

AnimationsModel *VirtualDesktopsData::animationsModel() const
{
    return m_animationsModel;
}

}
