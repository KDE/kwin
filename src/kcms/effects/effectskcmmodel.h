/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effectsmodel.h"

namespace KWin
{

class EffectsKcmModel : public EffectsModel
{
    Q_OBJECT

public:
    explicit EffectsKcmModel(QObject *parent = nullptr);

protected:
    void loadEffectsPostProcessing(QList<EffectData> &effects) override;
};
}
