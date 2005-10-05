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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */


#ifndef __KCM_H__
#define __KCM_H__

#include <kcmodule.h>
#include <kconfig.h>

class KConfig;
class KAboutData;

namespace KWinInternal
{

class KCMRulesList;

class KCMRules
    : public KCModule
    {
    Q_OBJECT
    public:
        KCMRules( QWidget *parent, const char *name );
        virtual void load();
        virtual void save();
        virtual void defaults();
        virtual QString quickHelp() const;
    protected slots:
        void moduleChanged( bool state );
    private:
        KCMRulesList* widget;
        KConfig config;
    };

} // namespace

#endif
