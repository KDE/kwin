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
