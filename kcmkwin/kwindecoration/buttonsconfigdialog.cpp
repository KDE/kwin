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
#include "buttonsconfigdialog.h"
#include "kwindecoration.h"

#include <QVBoxLayout>

#include <KLocale>
#include <kdecoration.h>

namespace KWin
{

KWinDecorationButtonsConfigForm::KWinDecorationButtonsConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinDecorationButtonsConfigDialog::KWinDecorationButtonsConfigDialog(DecorationButtons const *buttons, bool showTooltips, QWidget* parent, Qt::WFlags flags)
    : KDialog(parent, flags)
    , m_showTooltip(showTooltips)
    , m_buttons(buttons)
{
    m_ui = new KWinDecorationButtonsConfigForm(this);
    setWindowTitle(i18n("Buttons"));
    setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Default | KDialog::Reset);
    enableButton(KDialog::Reset, false);
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(m_ui);
    m_ui->buttonPositionWidget->setEnabled(buttons->customPositions());

    QWidget* main = new QWidget(this);
    main->setLayout(layout);
    setMainWidget(main);

    connect(m_ui->buttonPositionWidget, SIGNAL(changed()), this, SLOT(changed()));
    connect(m_ui->showToolTipsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->useCustomButtonPositionsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(this, SIGNAL(defaultClicked()), this, SLOT(slotDefaultClicked()));
    connect(this, SIGNAL(resetClicked()), this, SLOT(slotResetClicked()));

    slotResetClicked();
}

KWinDecorationButtonsConfigDialog::~KWinDecorationButtonsConfigDialog()
{
}

bool KWinDecorationButtonsConfigDialog::customPositions() const
{
    return m_ui->useCustomButtonPositionsCheckBox->isChecked();
}

bool KWinDecorationButtonsConfigDialog::showTooltips() const
{
    return m_ui->showToolTipsCheckBox->isChecked();
}

QString KWinDecorationButtonsConfigDialog::buttonsLeft() const
{
    return m_ui->buttonPositionWidget->buttonsLeft();
}

QString KWinDecorationButtonsConfigDialog::buttonsRight() const
{
    return m_ui->buttonPositionWidget->buttonsRight();
}

void KWinDecorationButtonsConfigDialog::changed()
{
    enableButton(KDialog::Reset, true);
}

void KWinDecorationButtonsConfigDialog::slotDefaultClicked()
{
    m_ui->useCustomButtonPositionsCheckBox->setChecked(false);
    m_ui->showToolTipsCheckBox->setChecked(true);
    m_ui->buttonPositionWidget->setButtonsLeft(KDecorationOptions::defaultTitleButtonsLeft());
    m_ui->buttonPositionWidget->setButtonsRight(KDecorationOptions::defaultTitleButtonsRight());
    changed();
}

void KWinDecorationButtonsConfigDialog::slotResetClicked()
{
    m_ui->useCustomButtonPositionsCheckBox->setChecked(m_buttons->customPositions());
    m_ui->showToolTipsCheckBox->setChecked(m_showTooltip);
    m_ui->buttonPositionWidget->setButtonsLeft(m_buttons->leftButtons());
    m_ui->buttonPositionWidget->setButtonsRight(m_buttons->rightButtons());
    changed();
    enableButton(KDialog::Reset, false);
}

} // namespace KWin

#include "buttonsconfigdialog.moc"
