/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2018 Vlad Zahorodnii <vladzzag@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
