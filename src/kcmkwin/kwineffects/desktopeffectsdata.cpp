/*
    SPDX-FileCopyrightText: 2021 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopeffectsdata.h"
#include "effectsmodel.h"

namespace KWin
{

DesktopEffectsData::DesktopEffectsData(QObject *parent, const QVariantList &args)
    : KCModuleDataSignaling(parent, args)
    , m_model(new EffectsModel(this))

{
    connect(m_model, &EffectsModel::loaded, this, &KCModuleDataSignaling::loaded);

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
