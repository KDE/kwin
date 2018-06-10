/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef SCALE_CONFIG_H
#define SCALE_CONFIG_H

#include "ui_scale_config.h"

#include <KCModule>

namespace KWin
{

class ScaleEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit ScaleEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~ScaleEffectConfig() override;

    void save() override;

private:
    ::Ui::ScaleEffectConfig ui;
};

} // namespace KWin

#endif

