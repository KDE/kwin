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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */

#ifndef __KKWMMOUSECONFIG_H__
#define __KKWMMOUSECONFIG_H__

class KConfig;

#include <qwidget.h>
#include <kcmodule.h>
#include <qcombobox.h>
#include <qtooltip.h>


class ToolTipComboBox: public QComboBox
{
  Q_OBJECT
    
public:
  ToolTipComboBox(QWidget * owner, char const * const * toolTips_)
    : QComboBox(owner)
    , toolTips(toolTips_) {}

public slots:
  void changed() {this->setToolTip( i18n(toolTips[currentItem()]) );}

protected:
  char const * const * toolTips;
};



class KTitleBarActionsConfig : public KCModule
{
  Q_OBJECT

public:

  KTitleBarActionsConfig( bool _standAlone, KConfig *_config, KInstance *inst, QWidget *parent );
  ~KTitleBarActionsConfig( );

  void load();
  void save();
  void defaults();

public slots:
	void changed() { emit KCModule::changed(true); }

private:
  QComboBox* coTiDbl;

  QComboBox* coTiAct1;
  QComboBox* coTiAct2;
  QComboBox* coTiAct3;
  QComboBox* coTiAct4;  
  QComboBox* coTiInAct1;
  QComboBox* coTiInAct2;
  QComboBox* coTiInAct3;

  ToolTipComboBox * coMax[3];

  KConfig *config;
  bool standAlone;

  const char* functionTiDbl(int);
  const char* functionTiAc(int);
  const char* functionTiWAc(int);  
  const char* functionTiInAc(int);
  const char* functionMax(int);

  void setComboText(QComboBox* combo, const char* text);
  const char* fixup( const char* s );

private slots:
  void paletteChanged();

};

class KWindowActionsConfig : public KCModule
{
  Q_OBJECT

public:

  KWindowActionsConfig( bool _standAlone, KConfig *_config, KInstance *inst, QWidget *parent );
  ~KWindowActionsConfig( );

  void load();
  void save();
  void defaults();

public slots:
	void changed() { emit KCModule::changed(true); }

private:
  QComboBox* coWin1;
  QComboBox* coWin2;
  QComboBox* coWin3;

  QComboBox* coAllKey;
  QComboBox* coAll1;
  QComboBox* coAll2;
  QComboBox* coAll3;
  QComboBox* coAllW;

  KConfig *config;
  bool standAlone;

  const char* functionWin(int);
  const char* functionAllKey(int);
  const char* functionAll(int);
  const char* functionAllW(int);

  void setComboText(QComboBox* combo, const char* text);
  const char* fixup( const char* s );
};

#endif

