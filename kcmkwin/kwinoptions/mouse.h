/*
 * mouse.h
 *
 * Copyright (c) 1998 Matthias Ettrich <ettrich@kde.org>
 *
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

#ifndef __KKWMMOUSECONFIG_H__
#define __KKWMMOUSECONFIG_H__

class QComboBox;
class KConfig;

#include <kcmodule.h>

class KActionsConfig : public KCModule
{
  Q_OBJECT

public:

  KActionsConfig( KConfig *_config, QWidget *parent=0, const char* name=0 );
  ~KActionsConfig( );

  void load();
  void save();
  void defaults();

private  slots:
    void slotChanged();

private:
  QComboBox* coTiDbl;

  QComboBox* coTiAct1;
  QComboBox* coTiAct2;
  QComboBox* coTiAct3;
  QComboBox* coTiInAct1;
  QComboBox* coTiInAct2;
  QComboBox* coTiInAct3;

  QComboBox* coWin1;
  QComboBox* coWin2;
  QComboBox* coWin3;

  QComboBox* coAllKey;
  QComboBox* coAll1;
  QComboBox* coAll2;
  QComboBox* coAll3;

  KConfig *config;

  const char* functionTiDbl(int);
  const char* functionTiAc(int);
  const char* functionTiInAc(int);
  const char* functionWin(int);
  const char* functionAllKey(int);
  const char* functionAll(int);

  void setComboText(QComboBox* combo, const char* text);

};

#endif

