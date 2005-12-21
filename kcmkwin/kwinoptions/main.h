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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __MAIN_H__
#define __MAIN_H__

#include <qtabwidget.h>
#include <kcmodule.h>

class KConfig;
class KFocusConfig;
class KTitleBarActionsConfig;
class KWindowActionsConfig;
class KAdvancedConfig;
class KTranslucencyConfig;

class KWinOptions : public KCModule
{
  Q_OBJECT

public:

  KWinOptions(KInstance *inst, QWidget *parent);
  virtual ~KWinOptions();

  void load();
  void save();
  void defaults();
  QString quickHelp() const;


protected slots:

  void moduleChanged(bool state);


private:

  QTabWidget   *tab;

  KFocusConfig *mFocus;
  KTitleBarActionsConfig *mTitleBarActions;
  KWindowActionsConfig *mWindowActions;
  KMovingConfig *mMoving;
  KAdvancedConfig *mAdvanced;
  KTranslucencyConfig *mTranslucency;

  KConfig *mConfig;
};

class KActionsOptions : public KCModule
{
  Q_OBJECT

public:

  KActionsOptions(KInstance *inst, QWidget *parent);
  virtual ~KActionsOptions();

  void load();
  void save();
  void defaults();

protected slots:

  void moduleChanged(bool state);


private:

  QTabWidget   *tab;

  KTitleBarActionsConfig *mTitleBarActions;
  KWindowActionsConfig *mWindowActions;

  KConfig *mConfig;
};

#endif
