/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "animationsmodel.h"

#include <KConfigGroup>

namespace KWin
{

AnimationsModel::AnimationsModel(QObject *parent)
    : EffectsModel(parent)
{
    connect(this, &EffectsModel::loaded, this, [this]() {
        setAnimationEnabled(modelAnimationEnabled());
        setAnimationIndex(modelAnimationIndex());
        loadDefaults();
    });
    connect(this, &AnimationsModel::animationIndexChanged, this, [this]() {
        const QModelIndex index_ = index(m_animationIndex, 0);
        if (!index_.isValid()) {
            return;
        }
        const bool configurable = index_.data(ConfigurableRole).toBool();
        if (configurable != m_currentConfigurable) {
            m_currentConfigurable = configurable;
            Q_EMIT currentConfigurableChanged();
        }
    });
}

bool AnimationsModel::animationEnabled() const
{
    return m_animationEnabled;
}

void AnimationsModel::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled != enabled) {
        m_animationEnabled = enabled;
        Q_EMIT animationEnabledChanged();
    }
}

int AnimationsModel::animationIndex() const
{
    return m_animationIndex;
}

void AnimationsModel::setAnimationIndex(int index)
{
    if (m_animationIndex != index) {
        m_animationIndex = index;
        Q_EMIT animationIndexChanged();
    }
}

bool AnimationsModel::currentConfigurable() const
{
    return m_currentConfigurable;
}

bool AnimationsModel::defaultAnimationEnabled() const
{
    return m_defaultAnimationEnabled;
}

int AnimationsModel::defaultAnimationIndex() const
{
    return m_defaultAnimationIndex;
}

bool AnimationsModel::shouldStore(const EffectData &data) const
{
    return data.untranslatedCategory.contains(
        QStringLiteral("Virtual Desktop Switching Animation"), Qt::CaseInsensitive);
}

EffectsModel::Status AnimationsModel::status(int row) const
{
    return Status(data(index(row, 0), static_cast<int>(StatusRole)).toInt());
}

void AnimationsModel::loadDefaults()
{
    for (int i = 0; i < rowCount(); ++i) {
        const QModelIndex rowIndex = index(i, 0);
        if (rowIndex.data(EnabledByDefaultRole).toBool()) {
            m_defaultAnimationEnabled = true;
            m_defaultAnimationIndex = i;
            Q_EMIT defaultAnimationEnabledChanged();
            Q_EMIT defaultAnimationIndexChanged();
            break;
        }
    }
}

bool AnimationsModel::modelAnimationEnabled() const
{
    for (int i = 0; i < rowCount(); ++i) {
        if (status(i) != Status::Disabled) {
            return true;
        }
    }

    return false;
}

int AnimationsModel::modelAnimationIndex() const
{
    for (int i = 0; i < rowCount(); ++i) {
        if (status(i) != Status::Disabled) {
            return i;
        }
    }

    return 0;
}

void AnimationsModel::load()
{
    EffectsModel::load();
}

void AnimationsModel::save()
{
    for (int i = 0; i < rowCount(); ++i) {
        const auto status = (m_animationEnabled && i == m_animationIndex)
            ? EffectsModel::Status::Enabled
            : EffectsModel::Status::Disabled;
        updateEffectStatus(index(i, 0), status);
    }

    EffectsModel::save();
}

void AnimationsModel::defaults()
{
    EffectsModel::defaults();
    setAnimationEnabled(modelAnimationEnabled());
    setAnimationIndex(modelAnimationIndex());
}

bool AnimationsModel::isDefaults() const
{
    // effect at m_animationIndex index may not be the current saved selected effect
    const bool enabledByDefault = index(m_animationIndex, 0).data(EnabledByDefaultRole).toBool();
    return enabledByDefault;
}

bool AnimationsModel::needsSave() const
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    for (int i = 0; i < rowCount(); ++i) {
        const QModelIndex index_ = index(i, 0);
        const bool enabledConfig = kwinConfig.readEntry(
            index_.data(ServiceNameRole).toString() + QLatin1String("Enabled"),
            index_.data(EnabledByDefaultRole).toBool());
        const bool enabled = (m_animationEnabled && i == m_animationIndex);

        if (enabled != enabledConfig) {
            return true;
        }
    }

    return false;
}

}
