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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qlayout.h>

#include <dcopclient.h>

#include <kapplication.h>
#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kgenericfactory.h>

#include "mouse.h"
#include "windows.h"

#include "main.h"

typedef KGenericFactory<KWinOptions, QWidget> KWinOptFactory;
K_EXPORT_COMPONENT_FACTORY( libkcm_kwinoptions, KWinOptFactory("kcmkwm") );
/*
extern "C" {
  KCModule *create_kwinoptions ( QWidget *parent, const char* name)
  {
    //CT there's need for decision: kwm or kwin?
    KGlobal::locale()->insertCatalogue("kcmkwm");
    return new KWinOptions( parent, name);
  }
}
*/
KWinOptions::KWinOptions(QWidget *parent, const char *name, const QStringList &)
  : KCModule(parent, name)
{
  mConfig = new KConfig("kwinrc", false, true);

  QVBoxLayout *layout = new QVBoxLayout(this);
  tab = new QTabWidget(this);
  layout->addWidget(tab);

  mFocus = new KFocusConfig(mConfig, this, "KWin Focus Config");
  tab->addTab(mFocus, i18n("&Focus"));
  connect(mFocus, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mActions = new KActionsConfig(mConfig, this, "KWin Actions");
  tab->addTab(mActions, i18n("&Actions"));
  connect(mActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mMoving = new KMovingConfig(mConfig, this, "KWin Moving");
  tab->addTab(mMoving, i18n("&Moving"));
  connect(mMoving, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

  mAdvanced = new KAdvancedConfig(mConfig, this, "KWin Advanced");
  tab->addTab(mAdvanced, i18n("Ad&vanced"));
  connect(mAdvanced, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));
}


void KWinOptions::load()
{
  mConfig->reparseConfiguration();
  mFocus->load();
  mActions->load();
  mMoving->load();
  mAdvanced->load();
}


void KWinOptions::save()
{
  mFocus->save();
  mActions->save();
  mMoving->save();
  mAdvanced->save();

  // Send signal to kwin
  mConfig->sync();
  if ( !kapp->dcopClient()->isAttached() )
      kapp->dcopClient()->attach();
  kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
}


void KWinOptions::defaults()
{
  mFocus->defaults();
  mActions->defaults();
  mMoving->defaults();
  mAdvanced->defaults();
}

QString KWinOptions::quickHelp() const
{
  return i18n("<h1>Window Behavior</h1> Here you can customize the way windows behave when being"
    " moved, resized or clicked on. You can also specify a focus policy as well as a placement"
    " policy for new windows. "
    " <p>Please note that this configuration will not take effect if you don't use"
    " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
    " for how to customize window behavior.");
}

void KWinOptions::moduleChanged(bool state)
{
  emit changed(state);
}


#include "main.moc"
