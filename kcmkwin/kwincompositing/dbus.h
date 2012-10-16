/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

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

#ifndef MAIN_ADAPTOR_H
#define MAIN_ADAPTOR_H

#include <QtDBus/QDBusAbstractAdaptor>
#include "main.h"

namespace KWin
{
class MainAdaptor : public QDBusAbstractAdaptor
{
   Q_OBJECT
   Q_CLASSINFO("D-Bus Interface", "org.kde.kwinCompositingDialog")

private:
    KWinCompositingConfig *m_config;

public:
    MainAdaptor(KWinCompositingConfig *config) : QDBusAbstractAdaptor(config), m_config(config) { }

public slots:
    Q_NOREPLY void warn(QString message, QString details, QString dontAgainKey) {
        m_config->warn(message, details, dontAgainKey);
    }
};
}
#endif