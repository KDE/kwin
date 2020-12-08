/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2010 Alexandre Pereira <pereira.alex@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GLIDE_CONFIG_H
#define GLIDE_CONFIG_H

#include <KCModule>
#include "ui_glide_config.h"

namespace KWin
{

class GlideEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit GlideEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~GlideEffectConfig() override;

    void save() override;

private:
    ::Ui::GlideEffectConfig ui;
};

} // namespace KWin

#endif

