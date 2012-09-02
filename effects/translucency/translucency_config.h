/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_TRANSLUCENCY_CONFIG_H
#define KWIN_TRANSLUCENCY_CONFIG_H

#include <kcmodule.h>

#include "ui_translucency_config.h"

namespace KWin
{

class TranslucencyEffectConfigForm : public QWidget, public Ui::TranslucencyEffectConfigForm
{
    Q_OBJECT
public:
    explicit TranslucencyEffectConfigForm(QWidget* parent);
};

class TranslucencyEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit TranslucencyEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

    virtual void save();

private:
    TranslucencyEffectConfigForm* m_ui;
};

} // namespace

#endif
