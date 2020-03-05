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

#ifndef __KWINSCREENEDGECONFIGFORM_H__
#define __KWINSCREENEDGECONFIGFORM_H__

#include "kwinscreenedge.h"

namespace Ui
{
class KWinScreenEdgesConfigUI;
}

namespace KWin
{

class KWinScreenEdgesConfigForm : public KWinScreenEdge
{
    Q_OBJECT

public:
    KWinScreenEdgesConfigForm(QWidget *parent = nullptr);
    ~KWinScreenEdgesConfigForm() override;

    // value is between 0. and 1.
    void setElectricBorderCornerRatio(double value);
    void setDefaultElectricBorderCornerRatio(double value);

    // return value between 0. and 1.
    double electricBorderCornerRatio() const;

    void reload() override;
    void setDefaults() override;

protected:
    Monitor *monitor() const override;
    bool isSaveNeeded() const override;
    bool isDefault() const override;

private Q_SLOTS:
    void sanitizeCooldown();
    void groupChanged();

private:
    // electricBorderCornerRatio value between 0. and 1.
    double m_referenceCornerRatio = 0.;
    double m_defaultCornerRatio = 0.;

    Ui::KWinScreenEdgesConfigUI *ui;
};

} // namespace

#endif
