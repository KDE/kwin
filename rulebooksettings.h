/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
