/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

class KWinCompositingSetting;

class KWinCompositingData : public KCModuleData
{
    Q_OBJECT

public:
    explicit KWinCompositingData(QObject *parent);

    bool isDefaults() const override;

private:
    KWinCompositingSetting *m_settings;
};
