/*
 * main.h
 *
 * Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef __MAIN_H__
#define __MAIN_H__

#include <qtabwidget.h>
#include <kcmodule.h>

class KConfig;
class KFocusConfig;
class KActionsConfig;
class KAdvancedConfig;
class KAboutData;

class KWinOptions : public KCModule
{
  Q_OBJECT

public:

  KWinOptions(QWidget *parent, const char *name);
  virtual ~KWinOptions();

  void load();
  void save();
  void defaults();
  QString quickHelp() const;
  const KAboutData* aboutData() const;


protected slots:

  void moduleChanged(bool state);


private:

  QTabWidget   *tab;

  KFocusConfig *mFocus;
  KActionsConfig *mActions;
  KMovingConfig *mMoving;
  KAdvancedConfig *mAdvanced;

  KConfig *mConfig;
};

#endif
