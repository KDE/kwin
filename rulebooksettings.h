/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020 Henri Chain <henri.chain@enioka.com>

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

#ifndef RULEBOOKSETTINGS_H
#define RULEBOOKSETTINGS_H

#include "rulebooksettingsbase.h"
#include <KSharedConfig>

namespace KWin
{
class Rules;
class RuleSettings;

class RuleBookSettings : public RuleBookSettingsBase
{
public:
    RuleBookSettings(KSharedConfig::Ptr config, QObject *parent = nullptr);
    RuleBookSettings(const QString &configname, KConfig::OpenFlags, QObject *parent = nullptr);
    RuleBookSettings(KConfig::OpenFlags, QObject *parent = nullptr);
    RuleBookSettings(QObject *parent = nullptr);
    void setRules(const QVector<Rules *> &);
    QVector<Rules *> rules();
    bool usrSave() override;
    void usrRead() override;

private:
    QVector<RuleSettings *> m_list;
};

}

#endif // RULEBOOKSETTINGS_H
