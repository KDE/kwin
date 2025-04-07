/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

namespace KWin
{

class AnimationsGlobalsSettings;
class EffectsModel;
class EffectsSubsetModel;

class AnimationsData : public KCModuleData
{
    Q_OBJECT

public:
    explicit AnimationsData(QObject *parent);
    ~AnimationsData() override;

    bool isDefaults() const override;

private:
    AnimationsGlobalsSettings *m_animationsGlobalsSettings;
    EffectsModel *m_model;
    EffectsSubsetModel *m_windowOpenCloseAnimations;
    EffectsSubsetModel *m_windowMaximizeAnimations;
    EffectsSubsetModel *m_windowMinimizeAnimations;
    EffectsSubsetModel *m_windowFullscreenAnimations;
    EffectsSubsetModel *m_peekDesktopAnimations;
    EffectsSubsetModel *m_virtualDesktopAnimations;
    EffectsSubsetModel *m_otherEffects;
};

} // namespace KWin
