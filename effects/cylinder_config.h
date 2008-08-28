/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_CYLINDER_CONFIG_H
#define KWIN_CYLINDER_CONFIG_H

#include <kcmodule.h>

#include "ui_cylinder_config.h"

class KFileDialog;

namespace KWin
{

class CylinderEffectConfigForm : public QWidget, public Ui::CylinderEffectConfigForm
{
    Q_OBJECT
    public:
        explicit CylinderEffectConfigForm(QWidget* parent);
};

class CylinderEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit CylinderEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

    public slots:
        virtual void save();
        virtual void load();
        virtual void defaults();

    private slots:
        void capsSelectionChanged();
    private:
        CylinderEffectConfigForm* m_ui;
        KActionCollection* m_actionCollection;
    };

} // namespace

#endif
