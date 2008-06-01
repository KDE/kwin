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

#include "desktopgrid_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>
#include <kconfiggroup.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif
namespace KWin
{

DesktopGridEffectConfig::DesktopGridEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);

    mSlide = new QCheckBox(i18n("Animate desktop changes"), this);
    connect(mSlide, SIGNAL(toggled(bool)), this, SLOT(changed()));
    layout->addWidget(mSlide);

    QHBoxLayout* comboLayout = new QHBoxLayout();
    QLabel* label = new QLabel(i18n("Activate when cursor is at a specific edge "
            "or corner of the screen:"), this);

    mActivateCombo = new QComboBox;
    mActivateCombo->addItem(i18n("Top"));
    mActivateCombo->addItem(i18n("Top-right"));
    mActivateCombo->addItem(i18n("Right"));
    mActivateCombo->addItem(i18n("Bottom-right"));
    mActivateCombo->addItem(i18n("Bottom"));
    mActivateCombo->addItem(i18n("Bottom-left"));
    mActivateCombo->addItem(i18n("Left"));
    mActivateCombo->addItem(i18n("Top-left"));
    mActivateCombo->addItem(i18n("None"));
    connect(mActivateCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    comboLayout->addWidget(label);
    comboLayout->addWidget(mActivateCombo);
    layout->addLayout(comboLayout);

    KActionCollection* actionCollection = new KActionCollection( this, componentData() );
    KAction* show = static_cast<KAction*>(actionCollection->addAction( "ShowDesktopGrid" ));
    show->setText( i18n("Show Desktop Grid" ));
    show->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));
    show->setProperty("isConfigurationAction", true);

    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor);

    layout->addStretch();
    }

DesktopGridEffectConfig::~DesktopGridEffectConfig()
    {
    kDebug();
    // Undo (only) unsaved changes to global key shortcuts
    mShortcutEditor->undoChanges();
    }

void DesktopGridEffectConfig::load()
    {
    kDebug();
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("DesktopGrid");
    mSlide->setChecked(conf.readEntry("Slide", true));

    int activateBorder = conf.readEntry("BorderActivate", (int)ElectricNone);
    if(activateBorder == (int)ElectricNone)
        activateBorder--;
    mActivateCombo->setCurrentIndex(activateBorder);

    emit changed(false);
    }

void DesktopGridEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("DesktopGrid");
    conf.writeEntry("Slide", mSlide->isChecked());

    int activateBorder = mActivateCombo->currentIndex();
    if(activateBorder == (int)ELECTRIC_COUNT)
        activateBorder = (int)ElectricNone;
    conf.writeEntry("BorderActivate", activateBorder);
    conf.sync();

    mShortcutEditor->save();    // undo() will restore to this state from now on

    emit changed(false);
    EffectsHandler::sendReloadMessage( "desktopgrid" );
    }

void DesktopGridEffectConfig::defaults()
    {
    kDebug() ;
    mSlide->setChecked(true);
    mActivateCombo->setCurrentIndex( (int)ElectricNone );
    mShortcutEditor->allDefault();
    emit changed(true);
    }

} // namespace

#include "desktopgrid_config.moc"
