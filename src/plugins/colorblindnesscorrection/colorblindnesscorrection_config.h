/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickManagedConfigModule>

class ColorBlindnessCorrectionSettings;
class ColorBlindnessCorrectionSettingsData;

namespace KWin
{
class ColorBlindnessCorrectionEffectConfig : public KQuickManagedConfigModule
{
    Q_OBJECT

    Q_PROPERTY(ColorBlindnessCorrectionSettings *settings READ settings CONSTANT)

public:
    explicit ColorBlindnessCorrectionEffectConfig(QObject *parent, const KPluginMetaData &metaData);
    ~ColorBlindnessCorrectionEffectConfig() override;

    ColorBlindnessCorrectionSettings *settings() const;

public Q_SLOTS:
    void save() override;

private:
    ColorBlindnessCorrectionSettingsData *m_data;

}; // namespace
}
