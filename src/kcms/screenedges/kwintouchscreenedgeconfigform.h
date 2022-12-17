/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
    std::unique_ptr<Ui::KWinTouchScreenConfigUi> ui;
};

} // namespace
