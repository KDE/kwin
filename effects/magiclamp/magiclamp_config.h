/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_MAGICLAMP_CONFIG_H
#define KWIN_MAGICLAMP_CONFIG_H

#include <kcmodule.h>

#include "ui_magiclamp_config.h"


namespace KWin
{

class MagicLampEffectConfigForm : public QWidget, public Ui::MagicLampEffectConfigForm
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfigForm(QWidget* parent);
};

class MagicLampEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

public slots:
    virtual void save();

private:
    MagicLampEffectConfigForm* m_ui;
};

} // namespace

#endif
