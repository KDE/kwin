/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rulebooksettings.h"
#include "rulesettings.h"

namespace KWin
{
RuleBookSettings::RuleBookSettings(KSharedConfig::Ptr config, QObject *parent)
    : RuleBookSettingsBase(config, parent)
{
    for (int i = 1; i <= count(); i++) {
        m_list.append(new RuleSettings(config, QString::number(i), this));
    }
}

RuleBookSettings::RuleBookSettings(const QString &configname, KConfig::OpenFlags flags, QObject *parent)
    : RuleBookSettings(KSharedConfig::openConfig(configname, flags), parent)
{
}

RuleBookSettings::RuleBookSettings(KConfig::OpenFlags flags, QObject *parent)
    : RuleBookSettings(QStringLiteral("kwinrulesrc"), flags, parent)
{
}

RuleBookSettings::RuleBookSettings(QObject *parent)
    : RuleBookSettings(KConfig::FullConfig, parent)
{
}

void RuleBookSettings::setRules(const QVector<Rules *> &rules)
{
    int i = 1;
    const int list_length = m_list.length();
    for (const auto &rule : rules) {
        RuleSettings *settings;
        if (i <= list_length) {
            settings = m_list[i - 1];
            settings->setDefaults();
        } else {
            // If there are more rules than in cache
            settings = new RuleSettings(this->sharedConfig(), QString::number(i), this);
            m_list.append(settings);
        }
        rule->write(settings);
        i++;
    }

    setCount(rules.length());
}

QVector<Rules *> RuleBookSettings::rules()
{
    QVector<Rules *> result;
    result.reserve(mCount);
    // mCount is always <= m_list.length()
    for (int i = 0; i < mCount; i++) {
        result.append(new Rules(m_list[i]));
    }
    return result;
}

bool RuleBookSettings::usrSave()
{
    bool result = true;
    for (const auto &settings : qAsConst(m_list)) {
        result &= settings->save();
    }
    int nRuleGroups = sharedConfig()->groupList().length() - 1;
    // Remove any extra groups currently in config
    for (int i = mCount + 1; i <= nRuleGroups; i++) {
        sharedConfig()->deleteGroup(QString::number(i));
    }
    return result;
}

void RuleBookSettings::usrRead()
{
    const int list_length = m_list.length();
    for (int i = 1; i <= mCount; i++) {
        if (i <= list_length) {
            m_list[i - 1]->load();
        } else {
            // If there are more groups than in cache
            m_list.append(new RuleSettings(sharedConfig(), QString::number(i), this));
        }
    }
}

}
