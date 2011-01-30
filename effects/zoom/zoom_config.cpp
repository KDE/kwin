/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Sebastian Sauer <sebsauer@kdab.com>

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

#include "zoom_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ZoomEffectConfigForm::ZoomEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

ZoomEffectConfig::ZoomEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new ZoomEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    connect(m_ui->zoomStepsSpinBox, SIGNAL(valueChanged(double)), this, SLOT(changed()));
    connect(m_ui->mousePointerComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->mouseTrackingComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->focusTrackingCheckBox, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->followFocusCheckBox, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, KComponentData("kwin"));
    actionCollection->setConfigGroup("Zoom");
    actionCollection->setConfigGlobal(true);

    KAction* a;
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomIn));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Equal));

    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomOut));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));

    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ActualSize));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomLeft"));
    a->setIcon(KIcon("go-previous"));
    a->setText(i18n("Move Left"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomRight"));
    a->setIcon(KIcon("go-next"));
    a->setText(i18n("Move Right"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomUp"));
    a->setIcon(KIcon("go-up"));
    a->setText(i18n("Move Up"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomDown"));
    a->setIcon(KIcon("go-down"));
    a->setText(i18n("Move Down"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));

    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToFocus"));
    a->setIcon(KIcon("view-restore"));
    a->setText(i18n("Move Mouse to Focus"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F5));

    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToCenter"));
    a->setIcon(KIcon("view-restore"));
    a->setText(i18n("Move Mouse to Center"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F6));

    m_ui->editor->addCollection(actionCollection);
    load();
}

ZoomEffectConfig::~ZoomEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void ZoomEffectConfig::load()
{
    KCModule::load();
    KConfigGroup conf = EffectsHandler::effectConfig("Zoom");
    m_ui->zoomStepsSpinBox->setValue(conf.readEntry("ZoomFactor", 1.2));
    m_ui->mousePointerComboBox->setCurrentIndex(conf.readEntry("MousePointer", 0));
    m_ui->mouseTrackingComboBox->setCurrentIndex(conf.readEntry("MouseTracking", 0));
    m_ui->focusTrackingCheckBox->setChecked(conf.readEntry("EnableFocusTracking", false));
    m_ui->followFocusCheckBox->setChecked(conf.readEntry("EnableFollowFocus", true));
    emit changed(false);
}

void ZoomEffectConfig::save()
{
    //KCModule::save();
    KConfigGroup conf = EffectsHandler::effectConfig("Zoom");
    conf.writeEntry("ZoomFactor", m_ui->zoomStepsSpinBox->value());
    conf.writeEntry("MousePointer", m_ui->mousePointerComboBox->currentIndex());
    conf.writeEntry("MouseTracking", m_ui->mouseTrackingComboBox->currentIndex());
    conf.writeEntry("EnableFocusTracking", m_ui->focusTrackingCheckBox->isChecked());
    conf.writeEntry("EnableFollowFocus", m_ui->followFocusCheckBox->isChecked());
    m_ui->editor->save(); // undo() will restore to this state from now on
    conf.sync();
    emit changed(false);
    EffectsHandler::sendReloadMessage("zoom");
}

void ZoomEffectConfig::defaults()
{
    m_ui->zoomStepsSpinBox->setValue(1.25);
    m_ui->mousePointerComboBox->setCurrentIndex(0);
    m_ui->mouseTrackingComboBox->setCurrentIndex(0);
    m_ui->focusTrackingCheckBox->setChecked(false);
    m_ui->followFocusCheckBox->setChecked(true);
    m_ui->editor->allDefault();
    emit changed(true);
}

} // namespace

#include "zoom_config.moc"
