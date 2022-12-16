/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_slide_config.h"
#include <KCModule>

namespace KWin
{

class SlideEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit SlideEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~SlideEffectConfig() override;

    void save() override;

private:
    ::Ui::SlideEffectConfig m_ui;
};

} // namespace KWin
