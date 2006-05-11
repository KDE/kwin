/*
 *
 * Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <QLayout>
//Added by qt3to4:
#include <QVBoxLayout>

#include <dcopclient.h>

#include <kapplication.h>
#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <kdialog.h>

#include "mouse.h"
#include "windows.h"

#include "main.h"

static KInstance *_kcmkwm = 0;

inline KInstance *inst() {
        if (_kcmkwm)
                return _kcmkwm;
        _kcmkwm = new KInstance("kcmkwm");
        return _kcmkwm;
}


extern "C"
{
	KDE_EXPORT KCModule *create_kwinfocus(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KFocusConfig(true, c, inst(), parent);
	}

	KDE_EXPORT KCModule *create_kwinactions(QWidget *parent, const char *name)
	{
		return new KActionsOptions( inst(), parent);
	}

	KDE_EXPORT KCModule *create_kwinmoving(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KMovingConfig(true, c, inst(), parent);
	}

	KDE_EXPORT KCModule *create_kwinadvanced(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KAdvancedConfig(true, c, inst(), parent);
	}
        
	KDE_EXPORT KCModule *create_kwintranslucency(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KTranslucencyConfig(true, c, inst(), parent);
	}

	KDE_EXPORT KCModule *create_kwinoptions ( QWidget *parent, const char* name)
	{
		return new KWinOptions( inst(), parent);
	}
}

KWinOptions::KWinOptions(KInstance *inst, QWidget *parent)
  : KCModule(inst, parent)
{
  mConfig = new KConfig("kwinrc", false, true);

  QVBoxLayout *layout = new QVBoxLayout(this);
  tab = new QTabWidget(this);
  layout->addWidget(tab);

  mFocus = new KFocusConfig(false, mConfig, inst, this);
  mFocus->setObjectName("KWin Focus Config");
  mFocus->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mFocus, i18n("&Focus"));
  connect(mFocus, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, inst, this);
  mTitleBarActions->setObjectName("KWin TitleBar Actions");
  mTitleBarActions->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
  connect(mTitleBarActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mWindowActions = new KWindowActionsConfig(false, mConfig, inst, this);
  mWindowActions->setObjectName("KWin Window Actions");
  mWindowActions->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mWindowActions, i18n("Window Actio&ns"));
  connect(mWindowActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mMoving = new KMovingConfig(false, mConfig, inst, this);
  mMoving->setObjectName("KWin Moving");
  mMoving->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mMoving, i18n("&Moving"));
  connect(mMoving, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mAdvanced = new KAdvancedConfig(false, mConfig, inst, this);
  mAdvanced->setObjectName("KWin Advanced");
  mAdvanced->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mAdvanced, i18n("Ad&vanced"));
  connect(mAdvanced, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mTranslucency = new KTranslucencyConfig(false, mConfig, inst, this);
  mTranslucency->setObjectName("KWin Translucency");
  mTranslucency->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mTranslucency, i18n("&Translucency"));
  connect(mTranslucency, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));
    
  KAboutData *about =
    new KAboutData(I18N_NOOP("kcmkwinoptions"), I18N_NOOP("Window Behavior Configuration Module"),
                  0, 0, KAboutData::License_GPL,
                  I18N_NOOP("(c) 1997 - 2002 KWin and KControl Authors"));

  about->addAuthor("Matthias Ettrich",0,"ettrich@kde.org");
  about->addAuthor("Waldo Bastian",0,"bastian@kde.org");
  about->addAuthor("Cristian Tibirna",0,"tibirna@kde.org");
  about->addAuthor("Matthias Kalle Dalheimer",0,"kalle@kde.org");
  about->addAuthor("Daniel Molkentin",0,"molkentin@kde.org");
  about->addAuthor("Wynn Wilkes",0,"wynnw@caldera.com");
  about->addAuthor("Pat Dowler",0,"dowler@pt1B1106.FSH.UVic.CA");
  about->addAuthor("Bernd Wuebben",0,"wuebben@kde.org");
  about->addAuthor("Matthias Hoelzer-Kluepfel",0,"hoelzer@kde.org");
  setAboutData(about);
}

KWinOptions::~KWinOptions()
{
  delete mConfig;
}

void KWinOptions::load()
{
  mConfig->reparseConfiguration();
  mFocus->load();
  mTitleBarActions->load();
  mWindowActions->load();
  mMoving->load();
  mAdvanced->load();
  mTranslucency->load();
  emit KCModule::changed( false );
}


void KWinOptions::save()
{
  mFocus->save();
  mTitleBarActions->save();
  mWindowActions->save();
  mMoving->save();
  mAdvanced->save();
  mTranslucency->save();

  emit KCModule::changed( false );
  // Send signal to kwin
  mConfig->sync();
  if ( !kapp->dcopClient()->isAttached() )
      kapp->dcopClient()->attach();
  kapp->dcopClient()->send("kwin*", "", "reconfigure()", QByteArray());
}


void KWinOptions::defaults()
{
  mFocus->defaults();
  mTitleBarActions->defaults();
  mWindowActions->defaults();
  mMoving->defaults();
  mAdvanced->defaults();
  mTranslucency->defaults();
}

QString KWinOptions::quickHelp() const
{
  return i18n("<h1>Window Behavior</h1> Here you can customize the way windows behave when being"
    " moved, resized or clicked on. You can also specify a focus policy as well as a placement"
    " policy for new windows."
    " <p>Please note that this configuration will not take effect if you do not use"
    " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
    " for how to customize window behavior.");
}

void KWinOptions::moduleChanged(bool state)
{
  emit KCModule::changed(state);
}


KActionsOptions::KActionsOptions(KInstance *inst, QWidget *parent)
  : KCModule(inst, parent)
{
  mConfig = new KConfig("kwinrc", false, true);

  QVBoxLayout *layout = new QVBoxLayout(this);
  tab = new QTabWidget(this);
  layout->addWidget(tab);

  mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, inst, this);
  mTitleBarActions->setObjectName("KWin TitleBar Actions");
  mTitleBarActions->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
  connect(mTitleBarActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mWindowActions = new KWindowActionsConfig(false, mConfig, inst, this);
  mWindowActions->setObjectName("KWin Window Actions");
  mWindowActions->layout()->setMargin( KDialog::marginHint() );
  tab->addTab(mWindowActions, i18n("Window Actio&ns"));
  connect(mWindowActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));
}

KActionsOptions::~KActionsOptions()
{
  delete mConfig;
}

void KActionsOptions::load()
{
  mTitleBarActions->load();
  mWindowActions->load();
  emit KCModule::changed( false );
}


void KActionsOptions::save()
{
  mTitleBarActions->save();
  mWindowActions->save();

  emit KCModule::changed( false );
  // Send signal to kwin
  mConfig->sync();
  if ( !kapp->dcopClient()->isAttached() )
      kapp->dcopClient()->attach();
  kapp->dcopClient()->send("kwin*", "", "reconfigure()", QByteArray());
}


void KActionsOptions::defaults()
{
  mTitleBarActions->defaults();
  mWindowActions->defaults();
}

void KActionsOptions::moduleChanged(bool state)
{
  emit KCModule::changed(state);
}

#include "main.moc"
