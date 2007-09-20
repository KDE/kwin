/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#ifndef ADVANCED_H
#define ADVANCED_H

#include <kdialog.h>

#include "ui_advanced.h"


namespace KWin
{

class CompositingPrefs;

class KWinAdvancedCompositingOptions : public KDialog
{
    Q_OBJECT
    public:
        KWinAdvancedCompositingOptions(QWidget* parent, KSharedConfigPtr config, CompositingPrefs* defaults);
        virtual ~KWinAdvancedCompositingOptions();

        void load();

    public slots:
        void changed();
        void save();
        void compositingModeChanged();
        void showConfirmDialog();

        void reinitKWinCompositing();

    private:
        KSharedConfigPtr mKWinConfig;
        Ui::KWinAdvancedCompositingOptions ui;
        CompositingPrefs* mDefaultPrefs;
        QMap<QString, QString> mPreviousConfig;
};

} // namespace

#endif
