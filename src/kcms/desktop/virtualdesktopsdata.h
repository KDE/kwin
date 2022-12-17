/*
    SPDX-FileCopyrightText: 2021 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

class VirtualDesktopsSettings;

namespace KWin
{

class AnimationsModel;
class DesktopsModel;

class VirtualDesktopsData : public KCModuleData
{
    Q_OBJECT

public:
    explicit VirtualDesktopsData(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    bool isDefaults() const override;

    VirtualDesktopsSettings *settings() const;
    DesktopsModel *desktopsModel() const;
    AnimationsModel *animationsModel() const;

private:
    VirtualDesktopsSettings *m_settings;
    DesktopsModel *m_desktopsModel;
    AnimationsModel *m_animationsModel;
};

}
