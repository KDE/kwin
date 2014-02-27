/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include <QDialogButtonBox>

#include <KDE/KLocalizedString>
#include <kdecoration.h>

namespace KWin
{

KWinDecorationButtonsConfigForm::KWinDecorationButtonsConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinDecorationButtonsConfigDialog::KWinDecorationButtonsConfigDialog(DecorationButtons const *buttons, bool showTooltips, QWidget* parent, Qt::WFlags flags)
    : QDialog(parent, flags)
    , m_ui(new KWinDecorationButtonsConfigForm(this))
    , m_showTooltip(showTooltips)
    , m_buttons(buttons)
{
    setWindowTitle(i18n("Buttons"));
    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Reset);
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(false);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(m_ui);
    layout->addWidget(m_buttonBox);
    m_ui->buttonPositionWidget->setEnabled(buttons->customPositions());

    setLayout(layout);

    connect(m_ui->buttonPositionWidget, SIGNAL(changed()), this, SLOT(changed()));
    connect(m_ui->showToolTipsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->useCustomButtonPositionsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_buttonBox->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked(bool)), this, SLOT(slotDefaultClicked()));
    connect(m_buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked(bool)), this, SLOT(slotResetClicked()));
    connect(m_buttonBox, SIGNAL(accepted()), SLOT(accept()));
    connect(m_buttonBox, SIGNAL(rejected()), SLOT(reject()));

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

QList<KDecorationDefines::DecorationButton> KWinDecorationButtonsConfigDialog::buttonsLeft() const
{
    return m_ui->buttonPositionWidget->buttonsLeft();
}

QList<KDecorationDefines::DecorationButton> KWinDecorationButtonsConfigDialog::buttonsRight() const
{
    return m_ui->buttonPositionWidget->buttonsRight();
}

void KWinDecorationButtonsConfigDialog::changed()
{
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(true);
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

    m_buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
}

} // namespace KWin

#include "buttonsconfigdialog.moc"
