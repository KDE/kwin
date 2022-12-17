/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effectsmodel.h"

namespace KWin
{

class AnimationsModel : public EffectsModel
{
    Q_OBJECT
    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(int animationIndex READ animationIndex WRITE setAnimationIndex NOTIFY animationIndexChanged)
    Q_PROPERTY(bool currentConfigurable READ currentConfigurable NOTIFY currentConfigurableChanged)
    Q_PROPERTY(bool defaultAnimationEnabled READ defaultAnimationEnabled NOTIFY defaultAnimationEnabledChanged)
    Q_PROPERTY(int defaultAnimationIndex READ defaultAnimationIndex NOTIFY defaultAnimationIndexChanged)

public:
    explicit AnimationsModel(QObject *parent = nullptr);

    bool animationEnabled() const;
    void setAnimationEnabled(bool enabled);

    int animationIndex() const;
    void setAnimationIndex(int index);

    bool currentConfigurable() const;

    bool defaultAnimationEnabled() const;
    int defaultAnimationIndex() const;

    void load();
    void save();
    void defaults();
    bool isDefaults() const;
    bool needsSave() const;

Q_SIGNALS:
    void animationEnabledChanged();
    void animationIndexChanged();
    void currentConfigurableChanged();
    void defaultAnimationEnabledChanged();
    void defaultAnimationIndexChanged();

protected:
    bool shouldStore(const EffectData &data) const override;

private:
    Status status(int row) const;
    void loadDefaults();
    bool modelAnimationEnabled() const;
    int modelAnimationIndex() const;

    bool m_animationEnabled = false;
    bool m_defaultAnimationEnabled = false;
    int m_animationIndex = -1;
    int m_defaultAnimationIndex = -1;
    bool m_currentConfigurable = false;

    Q_DISABLE_COPY(AnimationsModel)
};

}
