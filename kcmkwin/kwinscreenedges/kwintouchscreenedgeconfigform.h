/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2020 Cyril Rossi <cyril.rossi@enioka.com>

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

#ifndef __KWINTOUCHSCREENEDGECONFIGFORM_H__
#define __KWINTOUCHSCREENEDGECONFIGFORM_H__

#include "kwinscreenedge.h"

namespace Ui
{
class KWinTouchScreenConfigUi;
}

namespace KWin
{

class KWinTouchScreenEdgeConfigForm : public KWinScreenEdge
{
    Q_OBJECT

public:
    KWinTouchScreenEdgeConfigForm(QWidget *parent = nullptr);
    ~KWinTouchScreenEdgeConfigForm() override;

protected:
    Monitor *monitor() const override;

private:
    Ui::KWinTouchScreenConfigUi *ui;
};

} // namespace

#endif
