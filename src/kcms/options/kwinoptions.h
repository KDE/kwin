/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinoptions_settings.h"
#include <KQuickManagedConfigModule>

class KWinOptionsSettings;

class KWinOptions : public KQuickManagedConfigModule
{
    Q_OBJECT

    Q_PROPERTY(bool isPiPEnabled READ isPiPEnabled CONSTANT)
    Q_PROPERTY(KWinOptionsSettings *kwinSettings READ kwinSettings CONSTANT)

public:
    KWinOptions(QObject *parent, const KPluginMetaData &data);

    bool isPiPEnabled() const;
    KWinOptionsSettings *kwinSettings() const;

public Q_SLOTS:
    void save() override;

private:
    KWinOptionsSettings *m_kwinSettings;
};
