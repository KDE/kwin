/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effectsmodel.h"

#include "animationskdeglobalssettings.h"
#include "effectssubsetmodel.h"

#include "animationsdata.h"

namespace KWin
{

AnimationsData::AnimationsData(QObject *parent)
    : KCModuleData(parent)
{
    m_animationsGlobalsSettings = new AnimationsGlobalsSettings(this);

    m_model = new EffectsModel(this);
    m_model->load();
    m_windowOpenCloseAnimations = new EffectsSubsetModel(m_model, QStringLiteral("toplevel-open-close-animation"), this);
    m_windowMaximizeAnimations = new EffectsSubsetModel(m_model, QStringLiteral("maximize"), this);
    m_windowMinimizeAnimations = new EffectsSubsetModel(m_model, QStringLiteral("minimize"), this);
    m_windowFullscreenAnimations = new EffectsSubsetModel(m_model, QStringLiteral("fullscreen"), this);
    m_peekDesktopAnimations = new EffectsSubsetModel(m_model, QStringLiteral("show-desktop"), this);
    m_virtualDesktopAnimations = new EffectsSubsetModel(m_model, QStringLiteral("desktop-animations"), this);
    m_otherEffects = new EffectsSubsetModel(m_model, {"fadingpopups", "slidingpopups", "login", "logout"}, this);

    disconnect(this, &KCModuleData::aboutToLoad, nullptr, nullptr);
    connect(m_model, &EffectsModel::loaded, this, &KCModuleData::loaded);
}

AnimationsData::~AnimationsData()
{
}

bool AnimationsData::isDefaults() const
{
    return m_animationsGlobalsSettings->isDefaults()
        && m_windowOpenCloseAnimations->isDefaults()
        && m_windowMaximizeAnimations->isDefaults()
        && m_windowMinimizeAnimations->isDefaults()
        && m_windowFullscreenAnimations->isDefaults()
        && m_virtualDesktopAnimations->isDefaults()
        && m_peekDesktopAnimations->isDefaults()
        && m_otherEffects->isDefaults();
}

} // namespace KWin

#include "moc_animationsdata.cpp"
