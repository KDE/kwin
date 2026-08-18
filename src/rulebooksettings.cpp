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

static bool isMetaDataGroup(const QString &group)
{
    return group == "General" || group.startsWith('$');
}

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
    for (const RuleSettings *settings : std::as_const(m_list)) {
        if (settings->enabled()) {
            result.append(new Rules(settings));
        }
    }
    return result;
}

bool RuleBookSettings::usrSave()
{
    bool result = true;
    for (const auto &settings : std::as_const(m_list)) {
        result &= settings->save();
    }

    for (const auto groups = sharedConfig()->groupList(); const auto &group : groups) {
        if (isMetaDataGroup(group)) {
            continue;
        }

        const bool exists = std::ranges::any_of(m_list, [group](const auto &candidate) {
            return candidate->currentGroup() == group;
        });

        if (!exists) {
            sharedConfig()->deleteGroup(group);
        }
    }

    return result;
}

void RuleBookSettings::usrRead()
{
    // Reset the "count" property to the default value so it gets deleted from the config.
    // TODO Plasma 7: Drop it.
    mCount = 0;

    // kwinrulesrc used to collect stray window rules. Purge them to migrate the config to the new format.
    // TODO Plasma 7: Drop it.
    if (!mRuleGroupList.isEmpty()) {
        const KConfig kwinrulesrc(QStringLiteral("kwinrulesrc"), KConfig::SimpleConfig);
        for (const auto availableGroups = kwinrulesrc.groupList(); const QString &groupName : availableGroups) {
            if (isMetaDataGroup(groupName)) {
                continue;
            }
            if (!mRuleGroupList.contains(groupName)) {
                sharedConfig()->deleteGroup(groupName);
            }
        }

        mOrder = mRuleGroupList;
        mRuleGroupList.clear();
    }

    for (const auto availableGroups = sharedConfig()->groupList(); const QString &group : availableGroups) {
        if (isMetaDataGroup(group)) {
            continue;
        }
        if (!mOrder.contains(group)) {
            mOrder.prepend(group);
        }
    }

    qDeleteAll(m_list);
    m_list.clear();
    m_list.reserve(mOrder.count());

    for (const QString &groupName : std::as_const(mOrder)) {
        auto ruleSettings = new RuleSettings(sharedConfig(), groupName, this);
        m_list.append(ruleSettings);

        // TODO Plasma 7: Drop this migration path.
        if (const int noBorderRule = ruleSettings->noborderrule()) {
            ruleSettings->setDecorationpolicyrule(noBorderRule);
            ruleSettings->setDecorationpolicy(ruleSettings->noborder() ? DecorationPolicy::None : DecorationPolicy::Server);

            // Set the default values so the noborder rule gets removed from the config.
            ruleSettings->setNoborderrule(0);
            ruleSettings->setNoborder(false);
        }
    }

    if (isSaveNeeded()) {
        save();
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
    mOrder.insert(row, groupName);

    return settings;
}

void RuleBookSettings::removeRuleSettingsAt(int row)
{
    Q_ASSERT(row >= 0 && row < m_list.count());

    delete m_list.at(row);
    m_list.removeAt(row);
    mOrder.removeAt(row);
}

void RuleBookSettings::moveRuleSettings(int srcRow, int destRow)
{
    Q_ASSERT(srcRow >= 0 && srcRow < m_list.count() && destRow >= 0 && destRow < m_list.count());

    m_list.insert(destRow, m_list.takeAt(srcRow));
    mOrder.insert(destRow, mOrder.takeAt(srcRow));
}

QString RuleBookSettings::generateGroupName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}
