/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kcmrules.h"
#include "rulesettings.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include <KAboutData>
#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>

namespace KWin
{

KCMKWinRules::KCMKWinRules(QObject *parent, const QVariantList &arguments)
    : KQuickAddons::ConfigModule(parent, arguments)
    , m_ruleBookModel(new RuleBookModel(this))
    , m_rulesModel(new RulesModel(this))
{
    auto about = new KAboutData(QStringLiteral("kcm_kwinrules"),
                                i18n("Window Rules"),
                                QStringLiteral("1.0"),
                                QString(),
                                KAboutLicense::GPL);
    about->addAuthor(i18n("Ismael Asensio"),
                     i18n("Author"),
                     QStringLiteral("isma.af@gmail.com"));
    setAboutData(about);

    setQuickHelp(i18n("<p><h1>Window-specific Settings</h1> Here you can customize window settings specifically only"
                      " for some windows.</p>"
                      " <p>Please note that this configuration will not take effect if you do not use"
                      " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
                      " for how to customize window behavior.</p>"));

    QStringList argList;
    for (const QVariant &arg : arguments) {
        argList << arg.toString();
    }
    parseArguments(argList);

    connect(m_rulesModel, &RulesModel::descriptionChanged, this, [this] {
        if (m_editIndex.isValid()) {
            m_ruleBookModel->setDescriptionAt(m_editIndex.row(), m_rulesModel->description());
        }
    });
    connect(m_rulesModel, &RulesModel::dataChanged, this, [this] {
        Q_EMIT m_ruleBookModel->dataChanged(m_editIndex, m_editIndex, {});
    });
    connect(m_ruleBookModel, &RuleBookModel::dataChanged, this, &KCMKWinRules::updateNeedsSave);
}

void KCMKWinRules::parseArguments(const QStringList &args)
{
    // When called from window menu, "uuid" and "whole-app" are set in arguments list
    bool nextArgIsUuid = false;
    QUuid uuid = QUuid();

    // TODO: Use a better argument parser
    for (const QString &arg : args) {
        if (arg == QLatin1String("uuid")) {
            nextArgIsUuid = true;
        } else if (nextArgIsUuid) {
            uuid = QUuid(arg);
            nextArgIsUuid = false;
        } else if (arg.startsWith("uuid=")) {
            uuid = QUuid(arg.mid(strlen("uuid=")));
        } else if (arg == QLatin1String("whole-app")) {
            m_wholeApp = true;
        }
    }

    if (uuid.isNull()) {
        qDebug() << "Invalid window uuid.";
        return;
    }

    // Get the Window properties
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("getWindowInfo"));
    message.setArguments({uuid.toString()});
    QDBusPendingReply<QVariantMap> async = QDBusConnection::sessionBus().asyncCall(message);

    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this, [this, uuid](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        self->deleteLater();
        if (!reply.isValid() || reply.value().isEmpty()) {
            qDebug() << "Error retrieving properties for window" << uuid;
            return;
        }
        qDebug() << "Retrieved properties for window" << uuid;
        m_winProperties = reply.value();

        if (m_alreadyLoaded) {
            createRuleFromProperties();
        }
    });
}

void KCMKWinRules::load()
{
    m_ruleBookModel->load();

    if (!m_winProperties.isEmpty() && !m_alreadyLoaded) {
        createRuleFromProperties();
    } else {
        m_editIndex = QModelIndex();
        Q_EMIT editIndexChanged();
    }

    m_alreadyLoaded = true;

    updateNeedsSave();
}

void KCMKWinRules::save()
{
    m_ruleBookModel->save();

    // Notify kwin to reload configuration
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KCMKWinRules::updateNeedsSave()
{
    setNeedsSave(m_ruleBookModel->isSaveNeeded());
    Q_EMIT needsSaveChanged();
}

void KCMKWinRules::createRuleFromProperties()
{
    if (m_winProperties.isEmpty()) {
        return;
    }

    QModelIndex matchedIndex = findRuleWithProperties(m_winProperties, m_wholeApp);
    if (!matchedIndex.isValid()) {
        m_ruleBookModel->insertRow(0);
        fillSettingsFromProperties(m_ruleBookModel->ruleSettingsAt(0), m_winProperties, m_wholeApp);
        matchedIndex = m_ruleBookModel->index(0);
        updateNeedsSave();
    }

    editRule(matchedIndex.row());
    m_rulesModel->setSuggestedProperties(m_winProperties);

    m_winProperties.clear();
}

int KCMKWinRules::editIndex() const
{
    if (!m_editIndex.isValid()) {
        return -1;
    }
    return m_editIndex.row();
}

void KCMKWinRules::setRuleDescription(int index, const QString &description)
{
    if (index < 0 || index >= m_ruleBookModel->rowCount()) {
        return;
    }

    if (m_editIndex.row() == index) {
        m_rulesModel->setDescription(description);
        return;
    }
    m_ruleBookModel->setDescriptionAt(index, description);

    updateNeedsSave();
}

void KCMKWinRules::editRule(int index)
{
    if (index < 0 || index >= m_ruleBookModel->rowCount()) {
        return;
    }

    m_editIndex = m_ruleBookModel->index(index);
    Q_EMIT editIndexChanged();

    m_rulesModel->setSettings(m_ruleBookModel->ruleSettingsAt(m_editIndex.row()));

    // Set the active page to rules editor (0:RulesList, 1:RulesEditor)
    setCurrentIndex(1);
}

void KCMKWinRules::createRule()
{
    const int newIndex = m_ruleBookModel->rowCount();
    m_ruleBookModel->insertRow(newIndex);

    updateNeedsSave();

    editRule(newIndex);
}

void KCMKWinRules::removeRule(int index)
{
    if (index < 0 || index >= m_ruleBookModel->rowCount()) {
        return;
    }

    m_ruleBookModel->removeRow(index);

    Q_EMIT editIndexChanged();
    updateNeedsSave();
}

void KCMKWinRules::moveRule(int sourceIndex, int destIndex)
{
    const int lastIndex = m_ruleBookModel->rowCount() - 1;
    if (sourceIndex == destIndex
        || (sourceIndex < 0 || sourceIndex > lastIndex)
        || (destIndex < 0 || destIndex > lastIndex)) {
        return;
    }

    m_ruleBookModel->moveRow(QModelIndex(), sourceIndex, QModelIndex(), destIndex);

    Q_EMIT editIndexChanged();
    updateNeedsSave();
}

void KCMKWinRules::duplicateRule(int index)
{
    if (index < 0 || index >= m_ruleBookModel->rowCount()) {
        return;
    }

    const int newIndex = index + 1;
    const QString newDescription = i18n("Copy of %1", m_ruleBookModel->descriptionAt(index));

    m_ruleBookModel->insertRow(newIndex);
    m_ruleBookModel->setRuleSettingsAt(newIndex, *(m_ruleBookModel->ruleSettingsAt(index)));
    m_ruleBookModel->setDescriptionAt(newIndex, newDescription);

    updateNeedsSave();
}

void KCMKWinRules::exportToFile(const QUrl &path, const QList<int> &indexes)
{
    if (indexes.isEmpty()) {
        return;
    }

    const auto config = KSharedConfig::openConfig(path.toLocalFile(), KConfig::SimpleConfig);

    const QStringList groups = config->groupList();
    for (const QString &groupName : groups) {
        config->deleteGroup(groupName);
    }

    for (int index : indexes) {
        if (index < 0 || index > m_ruleBookModel->rowCount()) {
            continue;
        }
        const RuleSettings *origin = m_ruleBookModel->ruleSettingsAt(index);
        RuleSettings exported(config, origin->description());

        RuleBookModel::copySettingsTo(&exported, *origin);
        exported.save();
    }
}

void KCMKWinRules::importFromFile(const QUrl &path)
{
    const auto config = KSharedConfig::openConfig(path.toLocalFile(), KConfig::SimpleConfig);
    const QStringList groups = config->groupList();
    if (groups.isEmpty()) {
        return;
    }

    for (const QString &groupName : groups) {
        RuleSettings settings(config, groupName);

        const bool remove = settings.deleteRule();
        const QString importDescription = settings.description();
        if (importDescription.isEmpty()) {
            continue;
        }

        // Try to find a rule with the same description to replace
        int newIndex = -2;
        for (int index = 0; index < m_ruleBookModel->rowCount(); index++) {
            if (m_ruleBookModel->descriptionAt(index) == importDescription) {
                newIndex = index;
                break;
            }
        }

        if (remove) {
            m_ruleBookModel->removeRow(newIndex);
            continue;
        }

        if (newIndex < 0) {
            newIndex = m_ruleBookModel->rowCount();
            m_ruleBookModel->insertRow(newIndex);
        }

        m_ruleBookModel->setRuleSettingsAt(newIndex, settings);

        // Reset rule editor if the current rule changed when importing
        if (m_editIndex.row() == newIndex) {
            m_rulesModel->setSettings(m_ruleBookModel->ruleSettingsAt(newIndex));
        }
    }

    updateNeedsSave();
}

// Code adapted from original `findRule()` method in `kwin_rules_dialog::main.cpp`
QModelIndex KCMKWinRules::findRuleWithProperties(const QVariantMap &info, bool wholeApp) const
{
    const QString wmclass_class = info.value("resourceClass").toString();
    const QString wmclass_name = info.value("resourceName").toString();
    const QString role = info.value("role").toString();
    const NET::WindowType type = static_cast<NET::WindowType>(info.value("type").toInt());
    const QString title = info.value("caption").toString();
    const QString machine = info.value("clientMachine").toString();
    const bool isLocalHost = info.value("localhost").toBool();

    int bestMatchRow = -1;
    int bestMatchScore = 0;

    for (int row = 0; row < m_ruleBookModel->rowCount(); row++) {
        const RuleSettings *settings = m_ruleBookModel->ruleSettingsAt(row);

        // If the rule doesn't match try the next one
        const Rules rule = Rules(settings);
        /* clang-format off */
        if (!rule.matchWMClass(wmclass_class, wmclass_name)
                || !rule.matchType(type)
                || !rule.matchRole(role)
                || !rule.matchTitle(title)
                || !rule.matchClientMachine(machine, isLocalHost)) {
            continue;
        }
        /* clang-format on */

        if (settings->wmclassmatch() != Rules::ExactMatch) {
            continue; // too generic
        }

        // Now that the rule matches the window, check the quality of the match
        // It stablishes a quality depending on the match policy of the rule
        int score = 0;
        bool generic = true;

        // from now on, it matches the app - now try to match for a specific window
        if (settings->wmclasscomplete()) {
            score += 1;
            generic = false; // this can be considered specific enough (old X apps)
        }
        if (!wholeApp) {
            if (settings->windowrolematch() != Rules::UnimportantMatch) {
                score += settings->windowrolematch() == Rules::ExactMatch ? 5 : 1;
                generic = false;
            }
            if (settings->titlematch() != Rules::UnimportantMatch) {
                score += settings->titlematch() == Rules::ExactMatch ? 3 : 1;
                generic = false;
            }
            if (settings->types() != NET::AllTypesMask) {
                // Checks that type fits the mask, and only one of the types
                int bits = 0;
                for (unsigned int bit = 1; bit < 1U << 31; bit <<= 1) {
                    if (settings->types() & bit) {
                        ++bits;
                    }
                }
                if (bits == 1) {
                    score += 2;
                }
            }
            if (generic) { // ignore generic rules, use only the ones that are for this window
                continue;
            }
        } else {
            if (settings->types() == NET::AllTypesMask) {
                score += 2;
            }
        }

        if (score > bestMatchScore) {
            bestMatchRow = row;
            bestMatchScore = score;
        }
    }

    if (bestMatchRow < 0) {
        return QModelIndex();
    }
    return m_ruleBookModel->index(bestMatchRow);
}

// Code adapted from original `findRule()` method in `kwin_rules_dialog::main.cpp`
void KCMKWinRules::fillSettingsFromProperties(RuleSettings *settings, const QVariantMap &info, bool wholeApp) const
{
    const QString wmclass_class = info.value("resourceClass").toString();
    const QString wmclass_name = info.value("resourceName").toString();
    const QString role = info.value("role").toString();
    const NET::WindowType type = static_cast<NET::WindowType>(info.value("type").toInt());
    const QString title = info.value("caption").toString();
    const QString machine = info.value("clientMachine").toString();

    settings->setDefaults();

    if (wholeApp) {
        if (!wmclass_class.isEmpty()) {
            settings->setDescription(i18n("Application settings for %1", wmclass_class));
        }
        // TODO maybe exclude some types? If yes, then also exclude them when searching.
        settings->setTypes(NET::AllTypesMask);
        settings->setTitlematch(Rules::UnimportantMatch);
        settings->setClientmachine(machine); // set, but make unimportant
        settings->setClientmachinematch(Rules::UnimportantMatch);
        settings->setWindowrolematch(Rules::UnimportantMatch);
        if (wmclass_name == wmclass_class) {
            settings->setWmclasscomplete(false);
            settings->setWmclass(wmclass_class);
            settings->setWmclassmatch(Rules::ExactMatch);
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            settings->setWmclasscomplete(true);
            settings->setWmclass(QStringLiteral("%1 %2").arg(wmclass_name, wmclass_class));
            settings->setWmclassmatch(Rules::ExactMatch);
        }
        return;
    }

    if (!wmclass_class.isEmpty()) {
        settings->setDescription(i18n("Window settings for %1", wmclass_class));
    }
    if (type == NET::Unknown) {
        settings->setTypes(NET::NormalMask);
    } else {
        settings->setTypes(NET::WindowTypeMask(1 << type)); // convert type to its mask
    }
    settings->setTitle(title); // set, but make unimportant
    settings->setTitlematch(Rules::UnimportantMatch);
    settings->setClientmachine(machine); // set, but make unimportant
    settings->setClientmachinematch(Rules::UnimportantMatch);
    if (!role.isEmpty() && role != "unknown" && role != "unnamed") { // Qt sets this if not specified
        settings->setWindowrole(role);
        settings->setWindowrolematch(Rules::ExactMatch);
        if (wmclass_name == wmclass_class) {
            settings->setWmclasscomplete(false);
            settings->setWmclass(wmclass_class);
            settings->setWmclassmatch(Rules::ExactMatch);
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            settings->setWmclasscomplete(true);
            settings->setWmclass(QStringLiteral("%1 %2").arg(wmclass_name, wmclass_class));
            settings->setWmclassmatch(Rules::ExactMatch);
        }
    } else { // no role set
        if (wmclass_name != wmclass_class) {
            // WM_CLASS components differ - perhaps the app got -name argument
            settings->setWmclasscomplete(true);
            settings->setWmclass(QStringLiteral("%1 %2").arg(wmclass_name, wmclass_class));
            settings->setWmclassmatch(Rules::ExactMatch);
        } else {
            // This is a window that has no role set, and both components of WM_CLASS
            // match (possibly only differing in case), which most likely means either
            // the application doesn't give a damn about distinguishing its various
            // windows, or it's an app that uses role for that, but this window
            // lacks it for some reason. Use non-complete WM_CLASS matching, also
            // include window title in the matching, and pray it causes many more positive
            // matches than negative matches.
            // WM_CLASS components differ - perhaps the app got -name argument
            settings->setTitlematch(Rules::ExactMatch);
            settings->setWmclasscomplete(false);
            settings->setWmclass(wmclass_class);
            settings->setWmclassmatch(Rules::ExactMatch);
        }
    }
}

K_PLUGIN_CLASS_WITH_JSON(KCMKWinRules, "kcm_kwinrules.json");

} // namespace

#include "kcmrules.moc"
