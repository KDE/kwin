/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_showpaint_config.h"

namespace KWin
{

class ShowPaintEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit ShowPaintEffectConfig(QWidget *parent = nullptr, const QVariantList &args = {});
    ~ShowPaintEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    std::unique_ptr<Ui::ShowPaintEffectConfig> m_ui;
};

} // namespace KWin
