/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#ifndef KWIN_MAGNIFIER_CONFIG_H
#define KWIN_MAGNIFIER_CONFIG_H

#include <kcmodule.h>

#include "ui_magnifier_config.h"

class KActionCollection;

namespace KWin
{

class MagnifierEffectConfigForm : public QWidget, public Ui::MagnifierEffectConfigForm
{
    Q_OBJECT
public:
    explicit MagnifierEffectConfigForm(QWidget* parent);
};

class MagnifierEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MagnifierEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    virtual ~MagnifierEffectConfig();

    virtual void save();
    virtual void defaults();

private:
    MagnifierEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
