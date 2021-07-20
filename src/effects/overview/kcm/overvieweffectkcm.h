/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_overvieweffectkcm.h"

namespace KWin
{

class OverviewEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit OverviewEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~OverviewEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    ::Ui::OverviewEffectConfig ui;
};

} // namespace KWin
