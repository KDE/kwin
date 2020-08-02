/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
