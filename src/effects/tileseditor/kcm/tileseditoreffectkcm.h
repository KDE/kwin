/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_tileseditoreffectkcm.h"

namespace KWin
{

class TilesEditorEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit TilesEditorEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~TilesEditorEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    ::Ui::TilesEditorEffectConfig ui;
};

} // namespace KWin
