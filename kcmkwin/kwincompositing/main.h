/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>

#include <ksharedconfig.h>
#include <ktemporaryfile.h>

#include "ui_main.h"
#include "compositingprefs.h"
#include "ktimerdialog.h"

class KPluginSelector;
class QLabel;

namespace KWin
{

class ConfirmDialog : public KTimerDialog
{
Q_OBJECT
public:
    ConfirmDialog();
};

class KWinCompositingConfig : public KCModule
    {
    Q_OBJECT
    public:
        KWinCompositingConfig(QWidget *parent, const QVariantList &args);
        virtual ~KWinCompositingConfig();

        virtual QString quickHelp() const;

    public slots:
        virtual void load();
        virtual void save();
        virtual void defaults();

    private slots:
        void reparseConfiguration( const QByteArray& conf );
        void compositingEnabled(bool enabled);
        void currentTabChanged(int tab);
        void electricBorderSelectionChanged(int edge, int index);

    private:
        void loadGeneralTab();
        void loadEffectsTab();
        void loadAdvancedTab();
        void loadElectricBorders();
        void saveGeneralTab();
        void saveEffectsTab();
        void saveAdvancedTab();

        void initEffectSelector();

        bool effectEnabled( const QString& effect, const KConfigGroup& cfg ) const;

        void setupElectricBorders();
        void addItemToEdgesMonitor(const QString& item);
        void changeElectricBorder( ElectricBorder border, int index );
        ElectricBorder checkEffectHasElectricBorder( int index );
        void saveElectricBorders();

        void activateNewConfig();
        bool isConfirmationNeeded() const;
        bool sendKWinReloadSignal();

        void updateBackupWithNewConfig();
        void resetNewToBackupConfig();
        void deleteBackupConfigFile();

        Ui::KWinCompositingConfig ui;
        CompositingPrefs mDefaultPrefs;

        KSharedConfigPtr mNewConfig;
        KConfig* mBackupConfig;

        enum ElectricBorderEffects
            {
            NoEffect,
            PresentWindowsAll,
            PresentWindowsCurrent,
            DesktopGrid,
            Cube,
            Cylinder,
            Sphere
            };
    };

} // namespace

#endif
