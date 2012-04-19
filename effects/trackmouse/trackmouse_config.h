/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

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

#ifndef KWIN_TRACKMOUSE_CONFIG_H
#define KWIN_TRACKMOUSE_CONFIG_H

#include <kcmodule.h>

#include "ui_trackmouse_config.h"

class KActionCollection;
class KAction;

namespace KWin
{

class TrackMouseEffectConfigForm : public QWidget, public Ui::TrackMouseEffectConfigForm
{
    Q_OBJECT
public:
    explicit TrackMouseEffectConfigForm(QWidget* parent);
};

class TrackMouseEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit TrackMouseEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    ~TrackMouseEffectConfig();

public slots:
    virtual void save();
    virtual void load();
    virtual void defaults();
private slots:
    void shortcutChanged(const QKeySequence &seq);
private:
    TrackMouseEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
