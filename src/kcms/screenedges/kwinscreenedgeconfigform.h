/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinscreenedge.h"

namespace Ui
{
class KWinScreenEdgesConfigUI;
}

namespace KWin
{

class KWinScreenEdgesConfigForm : public KWinScreenEdge
{
    Q_OBJECT

public:
    KWinScreenEdgesConfigForm(QWidget *parent = nullptr);
    ~KWinScreenEdgesConfigForm() override;

    void setRemainActiveOnFullscreen(bool remainActive);
    bool remainActiveOnFullscreen() const;

    // value is between 0. and 1.
    void setElectricBorderCornerRatio(double value);
    void setDefaultElectricBorderCornerRatio(double value);

    // return value between 0. and 1.
    double electricBorderCornerRatio() const;

    void setElectricBorderCornerRatioEnabled(bool enable);

    void reload() override;
    void setDefaults() override;

public Q_SLOTS:
    void setDefaultsIndicatorsVisible(bool visible);

protected:
    Monitor *monitor() const override;
    bool isSaveNeeded() const override;
    bool isDefault() const override;

private Q_SLOTS:
    void sanitizeCooldown();
    void groupChanged();
    void updateDefaultIndicators();

private:
    bool m_remainActiveOnFullscreen = true;

    // electricBorderCornerRatio value between 0. and 1.
    double m_referenceCornerRatio = 0.;
    double m_defaultCornerRatio = 0.;

    bool m_defaultIndicatorVisible = false;

    std::unique_ptr<Ui::KWinScreenEdgesConfigUI> ui;
};

} // namespace
