/*
 * windows.h
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 * Copyright (c) 2001 Waldo Bastian bastian@kde.org
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

#ifndef __KWINDOWCONFIG_H__
#define __KWINDOWCONFIG_H__

#include <qwidget.h>
#include <kcmodule.h>
#include <config.h>

class QRadioButton;
class QCheckBox;
class QPushButton;
class QComboBox;
class QGroupBox;
class QLabel;
class QSlider;
class QButtonGroup;
class QSpinBox;
class QVButtonGroup;

class KColorButton;
class KIntNumInput;

#define TRANSPARENT 0
#define OPAQUE      1

#define CLICK_TO_FOCUS     0
#define FOCUS_FOLLOW_MOUSE 1

#define TITLEBAR_PLAIN  0
#define TITLEBAR_SHADED 1

#define RESIZE_TRANSPARENT  0
#define RESIZE_OPAQUE       1

#define SMART_PLACEMENT        0
#define CASCADE_PLACEMENT      1
#define RANDOM_PLACEMENT       2
#define CENTERED_PLACEMENT     3
#define ZEROCORNERED_PLACEMENT 4
#define INTERACTIVE_PLACEMENT  5
#define MANUAL_PLACEMENT       6

#define  CLICK_TO_FOCUS               0
#define  FOCUS_FOLLOWS_MOUSE          1
#define  FOCUS_UNDER_MOUSE            2
#define  FOCUS_STRICTLY_UNDER_MOUSE   3

class QSpinBox;

class KFocusConfig : public KCModule
{
  Q_OBJECT
public:
  KFocusConfig( bool _standAlone, KConfig *_config, QWidget *parent=0, const char* name=0 );
  ~KFocusConfig();

  void load();
  void save();
  void defaults();

private slots:
  void setDelayFocusEnabled();
  void setAutoRaiseEnabled();
  void autoRaiseOnTog(bool);//CT 23Oct1998
  void delayFocusOnTog(bool);
  void clickRaiseOnTog(bool);
	void changed() { emit KCModule::changed(true); }


private:

  int getFocus( void );
  int getAutoRaiseInterval( void );
  int getDelayFocusInterval( void );

  void setFocus(int);
  void setAutoRaiseInterval(int);
  void setAutoRaise(bool);
  void setDelayFocusInterval(int);
  void setDelayFocus(bool);
  void setClickRaise(bool);
  void setAltTabMode(bool);
  void setTraverseAll(bool);
  void setRollOverDesktops(bool);
  void setShowPopupinfo(bool);

  QButtonGroup *fcsBox;
  QComboBox *focusCombo;
  QCheckBox *autoRaiseOn;
  QCheckBox *delayFocusOn;
  QCheckBox *clickRaiseOn;
  KIntNumInput *autoRaise;
  KIntNumInput *delayFocus;

  QButtonGroup *kbdBox;
  QCheckBox    *altTabPopup;
  QCheckBox    *traverseAll;
  QCheckBox    *rollOverDesktops;
  QCheckBox    *showPopupinfo;

  KConfig *config;
  bool     standAlone;
};

class KMovingConfig : public KCModule
{
  Q_OBJECT
public:
  KMovingConfig( bool _standAlone, KConfig *config, QWidget *parent=0, const char* name=0 );
  ~KMovingConfig();

  void load();
  void save();
  void defaults();

private slots:
  void setMinimizeAnim( bool );
  void setMinimizeAnimSpeed( int );
	void changed() { emit KCModule::changed(true); }

private:
  int getMove( void );
  bool getMinimizeAnim( void );
  int getMinimizeAnimSpeed( void );
  int getResizeOpaque ( void );
  bool getGeometryTip( void ); //KS
  int getPlacement( void ); //CT

  void setMove(int);
  void setResizeOpaque(int);
  void setGeometryTip(bool); //KS
  void setPlacement(int); //CT
  void setMoveResizeMaximized(bool);

  QButtonGroup *windowsBox;
  QCheckBox *opaque;
  QCheckBox *resizeOpaqueOn;
  QCheckBox *geometryTipOn;
  QCheckBox* minimizeAnimOn;
  QSlider *minimizeAnimSlider;
  QLabel *minimizeAnimSlowLabel, *minimizeAnimFastLabel;
  QCheckBox *moveResizeMaximized;

  QComboBox *placementCombo;

  KConfig *config;
  bool     standAlone;

  int getBorderSnapZone();
  void setBorderSnapZone( int );
  int getWindowSnapZone();
  void setWindowSnapZone( int );

  QVButtonGroup *MagicBox;
  KIntNumInput *BrdrSnap, *WndwSnap;
  QCheckBox *OverlapSnap;

};

class KAdvancedConfig : public KCModule
{
  Q_OBJECT
public:
  KAdvancedConfig( bool _standAlone, KConfig *config, QWidget *parent=0, const char* name=0 );
  ~KAdvancedConfig();

  void load();
  void save();
  void defaults();

private slots:
  void shadeHoverChanged(bool);

  //copied from kcontrol/konq/kwindesktop, aleXXX
  void setEBorders();

  void changed() { emit KCModule::changed(true); }

private:

  int getShadeHoverInterval (void );
  void setAnimateShade(bool);
  void setShadeHover(bool);
  void setShadeHoverInterval(int);

  QCheckBox *animateShade;
  QButtonGroup *shBox;
  QCheckBox *shadeHoverOn;
  KIntNumInput *shadeHover;

  KConfig *config;
  bool     standAlone;

  int getElectricBorders( void );
  int getElectricBorderDelay();
  void setElectricBorders( int );
  void setElectricBorderDelay( int );

  QVButtonGroup *electricBox;
  QRadioButton *active_disable;
  QRadioButton *active_move;
  QRadioButton *active_always;
  KIntNumInput *delays;
  
  void setFocusStealing( int );

  QComboBox* focusStealing;
};

class KProcess;
class KTranslucencyConfig : public KCModule
{
  Q_OBJECT
public:
  KTranslucencyConfig( bool _standAlone, KConfig *config, QWidget *parent=0, const char* name=0 );
  ~KTranslucencyConfig();
  
  void load();
  void save();
  void defaults();
  
private:
  QCheckBox *useTranslucency;
  QCheckBox *activeWindowTransparency;
  QCheckBox *inactiveWindowTransparency;
  QCheckBox *movingWindowTransparency;
  QCheckBox *dockWindowTransparency;
  QCheckBox *keepAboveAsActive;
  QCheckBox *disableARGB;
  QCheckBox *fadeInWindows;
  QCheckBox *fadeOnOpacityChange;
  QCheckBox *useShadows;
  QCheckBox *removeShadowsOnResize;
  QCheckBox *removeShadowsOnMove;
  QGroupBox *sGroup;
  QComboBox *transMode;
//   QPushButton *xcompmgrButton;
  KIntNumInput *activeWindowOpacity;
  KIntNumInput *inactiveWindowOpacity;
  KIntNumInput *movingWindowOpacity;
  KIntNumInput *dockWindowOpacity;
  KIntNumInput *dockWindowShadowSize;
  KIntNumInput *activeWindowShadowSize;
  KIntNumInput *inactiveWindowShadowSize;
  KIntNumInput *shadowTopOffset;
  KIntNumInput *shadowLeftOffset;
  KIntNumInput *fadeInSpeed;
  KIntNumInput *fadeOutSpeed;
  KColorButton *shadowColor;
  KConfig *config;
  bool     standAlone;
  bool alphaActivated;
  bool resetKompmgr_;
  bool kompmgrAvailable();
  bool kompmgrAvailable_;
  KProcess *kompmgr;
  
private slots:
  void resetKompmgr();
  void showWarning(bool alphaActivated);

};
#endif
