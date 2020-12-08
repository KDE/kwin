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
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(bool currentConfigurable READ currentConfigurable NOTIFY currentConfigurableChanged)

public:
    explicit AnimationsModel(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

    int currentIndex() const;
    void setCurrentIndex(int index);

    bool currentConfigurable() const;

    void load();
    void save();
    void defaults();
    bool isDefaults() const;
    bool needsSave() const;

Q_SIGNALS:
    void enabledChanged();
    void currentIndexChanged();
    void currentConfigurableChanged();

protected:
    bool shouldStore(const EffectData &data) const override;

private:
    Status status(int row) const;
    bool modelCurrentEnabled() const;
    int modelCurrentIndex() const;

    bool m_enabled = false;
    int m_currentIndex = -1;
    bool m_currentConfigurable = false;

    Q_DISABLE_COPY(AnimationsModel)
};

}
