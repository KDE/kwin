/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "presentwindows_config.h"

#include <kwineffects.h>

#include <kgenericfactory.h>
#include <klocale.h>
#include <kdebug.h>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QComboBox>

KWIN_EFFECT_CONFIG( presentwindows, KWin::PresentWindowsEffectConfig )

namespace KWin
{

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QStringList& args) :
        KCModule(KGenericFactory<PresentWindowsEffectConfig>::componentData(), parent, args)
    {
    kDebug() << k_funcinfo << endl;

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(i18n("Activate when cursor is at a specific edge "
            "or corner of the screen:")), 0, 0, 1, 3);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Fixed), 1, 0, 2, 1);

    layout->addWidget(new QLabel("for windows on current desktop: "), 1, 1);
    mActivateCombo = new QComboBox;
    addItems(mActivateCombo);
    connect(mActivateCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    layout->addWidget(mActivateCombo, 1, 2);

    layout->addWidget(new QLabel("for windows all desktop: "), 2, 1);
    mActivateAllCombo = new QComboBox;
    addItems(mActivateAllCombo);
    connect(mActivateAllCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    layout->addWidget(mActivateAllCombo, 2, 2);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 3, 0, 1, 3);

    load();
    }

PresentWindowsEffectConfig::~PresentWindowsEffectConfig()
    {
    kDebug() << k_funcinfo << endl;
    }

void PresentWindowsEffectConfig::addItems(QComboBox* combo)
    {
    combo->addItem(i18n("Top"));
    combo->addItem(i18n("Top-right"));
    combo->addItem(i18n("Right"));
    combo->addItem(i18n("Bottom-right"));
    combo->addItem(i18n("Bottom"));
    combo->addItem(i18n("Bottom-left"));
    combo->addItem(i18n("Left"));
    combo->addItem(i18n("Top-left"));
    combo->addItem(i18n("None"));
    }

void PresentWindowsEffectConfig::load()
    {
    kDebug() << k_funcinfo << endl;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");
    int activateBorder = conf.readEntry("BorderActivate", (int)ElectricTopRight);
    if(activateBorder == (int)ElectricNone)
        activateBorder--;
    mActivateCombo->setCurrentIndex(activateBorder);

    int activateAllBorder = conf.readEntry("BorderActivateAll", (int)ElectricNone);
    if(activateAllBorder == (int)ElectricNone)
        activateAllBorder--;
    mActivateAllCombo->setCurrentIndex(activateAllBorder);
    emit changed(false);
    }

void PresentWindowsEffectConfig::save()
    {
    kDebug() << k_funcinfo << endl;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");

    int activateBorder = mActivateCombo->currentIndex();
    if(activateBorder == (int)ELECTRIC_COUNT)
        activateBorder = (int)ElectricNone;
    conf.writeEntry("BorderActivate", activateBorder);

    int activateAllBorder = mActivateAllCombo->currentIndex();
    if(activateAllBorder == (int)ELECTRIC_COUNT)
        activateAllBorder = (int)ElectricNone;
    conf.writeEntry("BorderActivateAll", activateAllBorder);
    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "presentwindows" );
    }

void PresentWindowsEffectConfig::defaults()
    {
    kDebug() << k_funcinfo << endl;
    mActivateCombo->setCurrentIndex( (int)ElectricTopRight );
    mActivateAllCombo->setCurrentIndex( (int)ElectricNone - 1 );
    emit changed(true);
    }


} // namespace

#include "presentwindows_config.moc"
