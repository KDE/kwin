/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
Copyright (C) 2018 Vlad Zahorodnii <vladzzag@gmail.com>

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

#ifndef KWIN_DIMINACTIVE_CONFIG_H
#define KWIN_DIMINACTIVE_CONFIG_H

#include <KCModule>

#include "ui_diminactive_config.h"

namespace KWin
{

class DimInactiveEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit DimInactiveEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~DimInactiveEffectConfig() override;

    void save() override;

private:
    ::Ui::DimInactiveEffectConfig m_ui;
};

} // namespace KWin

#endif
