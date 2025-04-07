/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KQuickConfigModule>

namespace KWin
{

class AnimationsGlobalsSettings;
class EffectsModel;
class EffectsSubsetModel;

class AnimationsKCM : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(AnimationsGlobalsSettings *globalsSettings READ globalsSettings CONSTANT)
    Q_PROPERTY(EffectsSubsetModel *windowOpenCloseAnimations READ windowOpenCloseAnimations CONSTANT)
    Q_PROPERTY(EffectsSubsetModel *windowMinimizeAnimations READ windowMinimizeAnimations CONSTANT)
    Q_PROPERTY(EffectsSubsetModel *peekDesktopAnimations READ peekDesktopAnimations CONSTANT)
    Q_PROPERTY(EffectsSubsetModel *virtualDesktopAnimations READ virtualDesktopAnimations CONSTANT)

public:
    AnimationsKCM(QObject *parent, const KPluginMetaData &metaData);
    ~AnimationsKCM() override;

    AnimationsGlobalsSettings *globalsSettings() const;
    EffectsSubsetModel *windowOpenCloseAnimations() const;
    EffectsSubsetModel *windowMinimizeAnimations() const;
    EffectsSubsetModel *peekDesktopAnimations() const;
    EffectsSubsetModel *virtualDesktopAnimations() const;

    Q_INVOKABLE void configure(const QString &pluginId, QQuickItem *context) const;
    Q_INVOKABLE QVariantMap effectsKCMData() const;
    Q_INVOKABLE void launchEffectsKCM() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void updateNeedsSave();

private:
    AnimationsGlobalsSettings *m_animationsGlobalsSettings;
    EffectsModel *m_model;
    EffectsSubsetModel *m_windowOpenCloseAnimations;
    EffectsSubsetModel *m_windowMinimizeAnimations;
    EffectsSubsetModel *m_peekDesktopAnimations;
    EffectsSubsetModel *m_virtualDesktopAnimations;

    Q_DISABLE_COPY(AnimationsKCM)
};

} // namespace KWin
