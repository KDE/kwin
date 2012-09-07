/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "translucency_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>

#include <QVBoxLayout>
#include <QCheckBox>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

TranslucencyEffectConfigForm::TranslucencyEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

TranslucencyEffectConfig::TranslucencyEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new TranslucencyEffectConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    connect(m_ui->decorations, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->inactive, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->moveresize, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->dialogs, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->comboboxpopup, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->menus, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->individualmenuconfig, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->dropdownmenus, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->popupmenus, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->tornoffmenus, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    load();
}

void TranslucencyEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Translucency");
    m_ui->decorations->setValue((int)(conf.readEntry("Decoration", 1.0) * 100));
    m_ui->moveresize->setValue((int)(conf.readEntry("MoveResize", 0.8) * 100));
    m_ui->dialogs->setValue((int)(conf.readEntry("Dialogs", 1.0) * 100));
    m_ui->inactive->setValue((int)(conf.readEntry("Inactive", 1.0) * 100));
    m_ui->comboboxpopup->setValue((int)(conf.readEntry("ComboboxPopups", 1.0) * 100));
    m_ui->menus->setValue((int)(conf.readEntry("Menus", 1.0) * 100));
    m_ui->individualmenuconfig->setChecked((conf.readEntry("IndividualMenuConfig", false)));
    m_ui->dropdownmenus->setValue((int)(conf.readEntry("DropdownMenus", 1.0) * 100));
    m_ui->popupmenus->setValue((int)(conf.readEntry("PopupMenus", 1.0) * 100));
    m_ui->tornoffmenus->setValue((int)(conf.readEntry("TornOffMenus", 1.0) * 100));

    emit changed(false);
}

void TranslucencyEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("Translucency");
    conf.writeEntry("Decoration", m_ui->decorations->value() / 100.0);
    conf.writeEntry("MoveResize", m_ui->moveresize->value() / 100.0);
    conf.writeEntry("Dialogs", m_ui->dialogs->value() / 100.0);
    conf.writeEntry("Inactive", m_ui->inactive->value() / 100.0);
    conf.writeEntry("ComboboxPopups", m_ui->comboboxpopup->value() / 100.0);
    conf.writeEntry("Menus", m_ui->menus->value() / 100.0);
    conf.writeEntry("IndividualMenuConfig", m_ui->individualmenuconfig->isChecked());
    conf.writeEntry("DropdownMenus", m_ui->dropdownmenus->value() / 100.0);
    conf.writeEntry("PopupMenus", m_ui->popupmenus->value() / 100.0);
    conf.writeEntry("TornOffMenus", m_ui->tornoffmenus->value() / 100.0);
    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("translucency");
}

void TranslucencyEffectConfig::defaults()
{
    m_ui->decorations->setValue(100);
    m_ui->moveresize->setValue(80);
    m_ui->dialogs->setValue(100);
    m_ui->inactive->setValue(100);
    m_ui->comboboxpopup->setValue(100);
    m_ui->menus->setValue(100);
    m_ui->individualmenuconfig->setChecked(false);
    m_ui->dropdownmenus->setValue(100);
    m_ui->popupmenus->setValue(100);
    m_ui->tornoffmenus->setValue(100);
    emit changed(true);
}


} // namespace

#include "translucency_config.moc"
