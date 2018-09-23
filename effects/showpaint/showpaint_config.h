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
    Ui::ShowPaintEffectConfig *m_ui;
};

} // namespace KWin
