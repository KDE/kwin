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
#include <kaboutdata.h>

#include "mouse.h"
#include "windows.h"

extern "C"
{
	KCModule *create_kwinfocus(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KFocusConfig(c, parent, name);
	}

	KCModule *create_kwinactions(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KActionsConfig(c, parent, name);
	}

	KCModule *create_kwinmoving(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KMovingConfig(c, parent, name);
	}

	KCModule *create_kwinadvanced(QWidget *parent, const char *name)
	{
		KConfig *c = new KConfig("kwinrc", false, true);
		return new KAdvancedConfig(c, parent, name);
	}

}


/*
  mConfig->reparseConfiguration();


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

QString KWinOptions::quickHelp() const
{
  return i18n("<h1>Window Behavior</h1> Here you can customize the way windows behave when being"
    " moved, resized or clicked on. You can also specify a focus policy as well as a placement"
    " policy for new windows."
    " <p>Please note that this configuration will not take effect if you don't use"
    " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
    " for how to customize window behavior.");
}

const KAboutData* KWinOptions::aboutData() const
{
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

    return about;
}

*/


