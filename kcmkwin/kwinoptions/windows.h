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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef KKWMWINDOWS_H
#define KKWMWINDOWS_H

#include <QWidget>
#include <kcmodule.h>
#include <config-workspace.h>

class QRadioButton;
class QCheckBox;
class QPushButton;
class KComboBox;
class QGroupBox;
class QLabel;
class QSlider;
class KButtonGroup;
// class QSpinBox;

class KColorButton;
class KIntNumInput;

#define CLICK_TO_FOCUS     0
#define FOCUS_FOLLOW_MOUSE 1

#define TITLEBAR_PLAIN  0
#define TITLEBAR_SHADED 1

#define  CLICK_TO_FOCUS               0
#define  FOCUS_FOLLOWS_MOUSE          1
#define  FOCUS_UNDER_MOUSE            2
#define  FOCUS_STRICTLY_UNDER_MOUSE   3

class KFocusConfig : public KCModule
{
    Q_OBJECT
public:
    KFocusConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent);
    ~KFocusConfig();

    void load();
    void save();
    void defaults();

protected:
    void showEvent(QShowEvent *ev);

private slots:
    void setDelayFocusEnabled();
    void focusPolicyChanged();
    void autoRaiseOnTog(bool);//CT 23Oct1998
    void delayFocusOnTog(bool);
    void clickRaiseOnTog(bool);
    void updateActiveMouseScreen();
    void changed() {
        emit KCModule::changed(true);
    }


private:

    int getFocus(void);
    int getAutoRaiseInterval(void);
    int getDelayFocusInterval(void);

    void setFocus(int);
    void setAutoRaiseInterval(int);
    void setAutoRaise(bool);
    void setDelayFocusInterval(int);
    void setClickRaise(bool);
    void setSeparateScreenFocus(bool);
    void setActiveMouseScreen(bool);

    void setFocusStealing(int);
    KComboBox* focusStealing;

    //QGroupBox *fcsBox;
    QWidget* fcsBox;
    KComboBox *focusCombo;
    QLabel *autoRaiseOnLabel;
    QCheckBox *autoRaiseOn;
    QLabel *delayFocusOnLabel;
    QCheckBox *clickRaiseOn;
    KIntNumInput *autoRaise;
    KIntNumInput *delayFocus;
    QCheckBox *separateScreenFocus;
    QCheckBox *activeMouseScreen;
    QWidget *focusNextToMouseContainer;
    QCheckBox *focusNextToMouse;

    KConfig *config;
    bool     standAlone;
};

class KMovingConfig : public KCModule
{
    Q_OBJECT
public:
    KMovingConfig(bool _standAlone, KConfig *config, const KComponentData &inst, QWidget *parent);
    ~KMovingConfig();

    void load();
    void save();
    void defaults();

protected:
    void showEvent(QShowEvent *ev);

private slots:
    void changed() {
        emit KCModule::changed(true);
    }
    void slotBrdrSnapChanged(int);
    void slotWndwSnapChanged(int);
    void slotCntrSnapChanged(int);

private:
    bool getGeometryTip(void);   //KS

    void setGeometryTip(bool); //KS
    void setMoveResizeMaximized(bool);

    KButtonGroup *windowsBox;
    QCheckBox *geometryTipOn;
    QCheckBox *moveResizeMaximized;

    KConfig *config;
    bool     standAlone;

    int getBorderSnapZone();
    void setBorderSnapZone(int);
    int getWindowSnapZone();
    void setWindowSnapZone(int);
    int getCenterSnapZone();
    void setCenterSnapZone(int);

    KButtonGroup *MagicBox;
    QLabel *BrdrSnapLabel, *WndwSnapLabel, *CntrSnapLabel;
    KIntNumInput *BrdrSnap, *WndwSnap, *CntrSnap;
    QCheckBox *OverlapSnap;

};

class KAdvancedConfig : public KCModule
{
    Q_OBJECT
public:
    KAdvancedConfig(bool _standAlone, KConfig *config, const KComponentData &inst, QWidget *parent);
    ~KAdvancedConfig();

    void load();
    void save();
    void defaults();

protected:
    void showEvent(QShowEvent *ev);

private slots:
    void shadeHoverChanged(bool);

    void changed() {
        emit KCModule::changed(true);
    }

private:

    int getShadeHoverInterval(void);
    void setShadeHover(bool);
    void setShadeHoverInterval(int);

    KButtonGroup *shBox;
    KButtonGroup *wtBox;
    QCheckBox *shadeHoverOn;
    QLabel *shadeHoverLabel;
    KIntNumInput *shadeHover;

    KConfig *config;
    bool     standAlone;

    void setHideUtilityWindowsForInactive(bool);
    QCheckBox* hideUtilityWindowsForInactive;

    void setInactiveTabsSkipTaskbar(bool);
    QCheckBox* inactiveTabsSkipTaskbar;

    void setAutogroupSimilarWindows(bool);
    QCheckBox* autogroupSimilarWindows;

    void setAutogroupInForeground(bool);
    QCheckBox* autogroupInForeground;

    KComboBox *placementCombo;
};

#endif // KKWMWINDOWS_H
