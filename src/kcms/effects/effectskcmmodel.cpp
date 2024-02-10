/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effectskcmmodel.h"

#include <KLocalizedString>

namespace KWin
{

EffectsKcmModel::EffectsKcmModel(QObject *parent)
    : EffectsModel(parent)
{
}

void EffectsKcmModel::loadEffectsPostProcessing(QList<EffectData> &effects)
{
    // Add an "off" option before each exclusive group
    QList<EffectData> t;
    QString exclusiveGroup;
    for (const auto &effect : effects) {
        if (effect.exclusiveGroup != exclusiveGroup) {
            exclusiveGroup = effect.exclusiveGroup;
            if (!exclusiveGroup.isEmpty()) {
                EffectData off;
                off.fake = true;
                off.name = i18nc("Disable all effects in an exclusive group", "Disable");
                off.exclusiveGroup = exclusiveGroup;
                off.category = effect.category;
                off.untranslatedCategory = effect.untranslatedCategory;
                off.enabledByDefault = false;
                off.enabledByDefaultFunction = false;
                t << off;
            }
        }
        t << effect;
    }

    // Set properties for "Off"
    exclusiveGroup.clear();
    bool allInternal = true;
    bool allUnsupported = true;
    bool allDisabled = true;
    for (auto it = t.rbegin(); it != t.rend(); ++it) {
        if (it->exclusiveGroup.isEmpty()) {
            continue;
        }

        if (it->fake) {
            it->internal = allInternal;
            it->supported = !allUnsupported;
            if (allDisabled) {
                it->status = Status::Enabled;
                it->originalStatus = Status::Enabled;
            } else {
                it->status = Status::Disabled;
                it->originalStatus = Status::Disabled;
            }
            allInternal = true;
            allUnsupported = true;
            allDisabled = true;
        } else {
            if (!it->internal) {
                allInternal = false;
            }
            if (it->supported) {
                allUnsupported = false;
            }
            if (it->status != Status::Disabled) {
                allDisabled = false;
            }
        }
    }

    effects.swap(t);
}

} // namespace KWin

#include "moc_effectskcmmodel.cpp"
