/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>
    SPDX-FileCopyrightText: 2021 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rulebooksettings.h"
#include "rulesettings.h"

#include <QUuid>

namespace KWin
{
RuleBookSettings::RuleBookSettings(KSharedConfig::Ptr config, QObject *parent)
    : RuleBookSettingsBase(config, parent)
{
}

RuleBookSettings::RuleBookSettings(QObject *parent)
    : RuleBookSettings(KSharedConfig::openConfig(QStringLiteral("kwinrulesrc"), KConfig::NoGlobals), parent)
{
}

RuleBookSettings::~RuleBookSettings()
{
    qDeleteAll(m_list);
}

QList<Rules *> RuleBookSettings::rules() const
{
    QList<Rules *> result;
    result.reserve(m_list.count());
    for (const auto &settings : std::as_const(m_list)) {
        result.append(new Rules(settings));
    }
    return result;
}

bool RuleBookSettings::usrSave()
{
    bool result = true;
    for (const auto &settings : std::as_const(m_list)) {
        result &= settings->save();
    }

    // Remove deleted groups from config
    for (const QString &groupName : std::as_const(m_storedGroups)) {
        if (sharedConfig()->hasGroup(groupName) && !mRuleGroupList.contains(groupName)) {
            sharedConfig()->deleteGroup(groupName);
        }
    }
    m_storedGroups = mRuleGroupList;

    return result;
}

void RuleBookSettings::usrRead()
{
    qDeleteAll(m_list);
    m_list.clear();

    // Legacy path for backwards compatibility with older config files without a rules list
    if (mRuleGroupList.isEmpty() && mCount > 0) {
        mRuleGroupList.reserve(mCount);
        for (int i = 1; i <= count(); i++) {
            mRuleGroupList.append(QString::number(i));
        }
        save(); // Save the generated ruleGroupList property
    }

    mCount = mRuleGroupList.count();
    m_storedGroups = mRuleGroupList;

    m_list.reserve(mRuleGroupList.count());
    for (const QString &groupName : std::as_const(mRuleGroupList)) {
        m_list.append(new RuleSettings(sharedConfig(), groupName, this));
    }
}

bool RuleBookSettings::usrIsSaveNeeded() const
{
    return isSaveNeeded() || std::any_of(m_list.cbegin(), m_list.cend(), [](const auto &settings) {
               return settings->isSaveNeeded();
           });
}

int RuleBookSettings::ruleCount() const
{
    return m_list.count();
}

std::optional<int> RuleBookSettings::indexForId(const QString &id) const
{
    for (int i = 0; i < m_list.count(); i++) {
        if (m_list.at(i)->currentGroup() == id) {
            return i;
        }
    }
    return std::nullopt;
}

RuleSettings *RuleBookSettings::ruleSettingsAt(int row) const
{
    Q_ASSERT(row >= 0 && row < m_list.count());
    return m_list.at(row);
}

RuleSettings *RuleBookSettings::insertRuleSettingsAt(int row)
{
    Q_ASSERT(row >= 0 && row < m_list.count() + 1);

    const QString groupName = generateGroupName();
    RuleSettings *settings = new RuleSettings(sharedConfig(), groupName, this);
    settings->setDefaults();

    m_list.insert(row, settings);
    mRuleGroupList.insert(row, groupName);
    mCount++;

    return settings;
}

void RuleBookSettings::removeRuleSettingsAt(int row)
{
    Q_ASSERT(row >= 0 && row < m_list.count());

    delete m_list.at(row);
    m_list.removeAt(row);
    mRuleGroupList.removeAt(row);
    mCount--;
}

void RuleBookSettings::moveRuleSettings(int srcRow, int destRow)
{
    Q_ASSERT(srcRow >= 0 && srcRow < m_list.count() && destRow >= 0 && destRow < m_list.count());

    m_list.insert(destRow, m_list.takeAt(srcRow));
    mRuleGroupList.insert(destRow, mRuleGroupList.takeAt(srcRow));
}

QString RuleBookSettings::generateGroupName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}
