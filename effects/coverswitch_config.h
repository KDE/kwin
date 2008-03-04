/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

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

#ifndef KWIN_COVERSWITCH_CONFIG_H
#define KWIN_COVERSWITCH_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_coverswitch_config.h"

class KActionCollection;

namespace KWin
{

class CoverSwitchEffectConfigForm : public QWidget, public Ui::CoverSwitchEffectConfigForm
{
    Q_OBJECT
    public:
        explicit CoverSwitchEffectConfigForm(QWidget* parent);
};

class CoverSwitchEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit CoverSwitchEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
        ~CoverSwitchEffectConfig();

    public slots:
        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        CoverSwitchEffectConfigForm* m_ui;
    };

} // namespace

#endif
