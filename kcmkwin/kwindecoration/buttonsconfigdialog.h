/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#ifndef KWINDECORATIONBUTTONSCONFIGDIALOG_H
#define KWINDECORATIONBUTTONSCONFIGDIALOG_H

#include <QWidget>
#include <KDialog>

#include "ui_buttons.h"

namespace KWin
{

class DecorationButtons;

class KWinDecorationButtonsConfigForm : public QWidget, public Ui::KWinDecorationButtonsConfigForm
{
    Q_OBJECT

public:
    explicit KWinDecorationButtonsConfigForm(QWidget* parent);
};

class KWinDecorationButtonsConfigDialog : public KDialog
{
    Q_OBJECT
public:
    KWinDecorationButtonsConfigDialog(DecorationButtons const *buttons, bool showTooltips, QWidget* parent = 0, Qt::WFlags flags = 0);
    ~KWinDecorationButtonsConfigDialog();

    bool customPositions() const;
    bool showTooltips() const;
    QString buttonsLeft() const;
    QString buttonsRight() const;

private slots:
    void changed();
    void slotDefaultClicked();
    void slotResetClicked();

private:
    KWinDecorationButtonsConfigForm* m_ui;
    bool m_showTooltip;
    DecorationButtons const *m_buttons;
};

} // namespace KWin

#endif // KWINDECORATIONCONFIGBUTTONSDIALOG_H
