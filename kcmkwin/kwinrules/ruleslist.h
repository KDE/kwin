/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */


#ifndef __RULESLIST_H__
#define __RULESLIST_H__

#include "ruleslistbase.h"

#include <q3valuevector.h>

#include "../../rules.h"

class Q3ListBoxItem;

namespace KWinInternal
{

class KCMRulesList
    : public KCMRulesListBase
    {
    Q_OBJECT
    public:
        KCMRulesList( QWidget* parent = NULL, const char* name = NULL );
        virtual ~KCMRulesList();
        void load();
        void save();
        void defaults();
    signals:
        void changed( bool );
    private slots:
        void newClicked();
        void modifyClicked();
        void deleteClicked();
        void moveupClicked();
        void movedownClicked();
        void activeChanged( Q3ListBoxItem* );
    private:
        QVector< Rules* > rules;
    };

} // namespace

#endif
