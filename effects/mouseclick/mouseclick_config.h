/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Filip Wieladek <wattos@gmail.com>

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

#ifndef KWIN_MOUSECLICK_CONFIG_H
#define KWIN_MOUSECLICK_CONFIG_H

#include <kcmodule.h>

#include "ui_mouseclick_config.h"

class KActionCollection;

namespace KWin
{

class MouseClickEffectConfigForm : public QWidget, public Ui::MouseClickEffectConfigForm
{
    Q_OBJECT
public:
    explicit MouseClickEffectConfigForm(QWidget* parent);
};

class MouseClickEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MouseClickEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~MouseClickEffectConfig() override;

    void save() override;

private:
    MouseClickEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
