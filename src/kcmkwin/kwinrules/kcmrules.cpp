/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kcmrules.h"

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

    connect(m_rulesModel, &RulesModel::descriptionChanged, this, [this]{
        if (m_editIndex.isValid()) {
            m_ruleBookModel->setDescriptionAt(m_editIndex.row(), m_rulesModel->description());
        }
    } );
    connect(m_rulesModel, &RulesModel::dataChanged, this, &KCMKWinRules::updateNeedsSave);
    connect(m_ruleBookModel, &RulesModel::dataChanged, this, &KCMKWinRules::updateNeedsSave);
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
            uuid = arg.mid(strlen("uuid="));
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
    setNeedsSave(false);

    if (!m_winProperties.isEmpty() && !m_alreadyLoaded) {
        createRuleFromProperties();
    } else {
        m_editIndex = QModelIndex();
        emit editIndexChanged();
    }

    m_alreadyLoaded = true;
}

void KCMKWinRules::save()
{
    saveCurrentRule();

    m_ruleBookModel->save();

    // Notify kwin to reload configuration
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KCMKWinRules::updateNeedsSave()
{
    setNeedsSave(true);
    emit needsSaveChanged();
}

void KCMKWinRules::createRuleFromProperties()
{
    if (m_winProperties.isEmpty()) {
        return;
    }

    QModelIndex matchedIndex = m_ruleBookModel->findRuleWithProperties(m_winProperties, m_wholeApp);
    if (!matchedIndex.isValid()) {
        m_ruleBookModel->insertRow(0);
        m_ruleBookModel->setRuleAt(0, ruleForProperties(m_winProperties, m_wholeApp));
        matchedIndex = m_ruleBookModel->index(0);
        updateNeedsSave();
    }

    editRule(matchedIndex.row());
    m_rulesModel->setSuggestedProperties(m_winProperties);

    m_winProperties.clear();
}

void KCMKWinRules::saveCurrentRule()
{
    if (m_editIndex.isValid() && needsSave()) {
        m_ruleBookModel->setRuleAt(m_editIndex.row(), m_rulesModel->exportToRules());
    }
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
    saveCurrentRule();

    m_editIndex = m_ruleBookModel->index(index);
    emit editIndexChanged();

    m_rulesModel->importFromRules(m_ruleBookModel->ruleAt(m_editIndex.row()));

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

    emit editIndexChanged();
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

    emit editIndexChanged();
    updateNeedsSave();
}

void KCMKWinRules::duplicateRule(int index)
{
    if (index < 0 || index >= m_ruleBookModel->rowCount()) {
        return;
    }

    Rules *newRule = new Rules(*(m_ruleBookModel->ruleAt(index)));
    const QString newDescription = i18n("Copy of %1", m_ruleBookModel->descriptionAt(index));

    m_ruleBookModel->insertRow(index + 1);
    m_ruleBookModel->setRuleAt(index + 1, newRule);
    m_ruleBookModel->setDescriptionAt(index + 1, newDescription);

    updateNeedsSave();
}

void KCMKWinRules::exportToFile(const QUrl &path, const QList<int> &indexes)
{
    if (indexes.isEmpty()) {
        return;
    }
    saveCurrentRule();

    const auto config = KSharedConfig::openConfig(path.toLocalFile(), KConfig::SimpleConfig);

    for (const QString &groupName : config->groupList()) {
        config->deleteGroup(groupName);
    }

    for (int index : indexes) {
        if (index < 0 || index > m_ruleBookModel->rowCount()) {
            continue;
        }
        const Rules *rule = m_ruleBookModel->ruleAt(index);
        RuleSettings settings(config, rule->description);
        settings.setDefaults();
        rule->write(&settings);
        settings.save();
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

        m_ruleBookModel->setRuleAt(newIndex, new Rules(&settings));

        // Reset rule editor if the current rule changed when importing
        if (m_editIndex.row() == newIndex) {
            m_rulesModel->importFromRules(m_ruleBookModel->ruleAt(m_editIndex.row()));
        }
    }

    updateNeedsSave();
}

// Code adapted from original `findRule()` method in `kwin_rules_dialog::main.cpp`
Rules *KCMKWinRules::ruleForProperties(const QVariantMap &windowProperties, bool wholeApp) const
{
    const QByteArray wmclass_class = windowProperties.value("resourceClass").toByteArray().toLower();
    const QByteArray wmclass_name = windowProperties.value("resourceName").toByteArray().toLower();
    const QByteArray role = windowProperties.value("role").toByteArray().toLower();
    const NET::WindowType type = static_cast<NET::WindowType>(windowProperties.value("type").toInt());
    const QString title = windowProperties.value("caption").toString();
    const QByteArray machine = windowProperties.value("clientMachine").toByteArray();

    Rules *rule = new Rules();

    if (wholeApp) {
        rule->description = i18n("Application settings for %1", QString::fromLatin1(wmclass_class));
        // TODO maybe exclude some types? If yes, then also exclude them when searching.
        rule->types = NET::AllTypesMask;
        rule->titlematch = Rules::UnimportantMatch;
        rule->clientmachine = machine; // set, but make unimportant
        rule->clientmachinematch = Rules::UnimportantMatch;
        rule->windowrolematch = Rules::UnimportantMatch;
        if (wmclass_name == wmclass_class) {
            rule->wmclasscomplete = false;
            rule->wmclass = wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            rule->wmclasscomplete = true;
            rule->wmclass = wmclass_name + ' ' + wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        }
        return rule;
    }

    rule->description = i18n("Window settings for %1", QString::fromLatin1(wmclass_class));
    if (type == NET::Unknown) {
        rule->types = NET::NormalMask;
    } else {
        rule->types = NET::WindowTypeMask(1 << type); // convert type to its mask
    }
    rule->title = title; // set, but make unimportant
    rule->titlematch = Rules::UnimportantMatch;
    rule->clientmachine = machine; // set, but make unimportant
    rule->clientmachinematch = Rules::UnimportantMatch;
    if (!role.isEmpty() && role != "unknown" && role != "unnamed") { // Qt sets this if not specified
        rule->windowrole = role;
        rule->windowrolematch = Rules::ExactMatch;
        if (wmclass_name == wmclass_class) {
            rule->wmclasscomplete = false;
            rule->wmclass = wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        } else {
            // WM_CLASS components differ - perhaps the app got -name argument
            rule->wmclasscomplete = true;
            rule->wmclass = wmclass_name + ' ' + wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        }
    } else { // no role set
        if (wmclass_name != wmclass_class) {
            rule->wmclasscomplete = true;
            rule->wmclass = wmclass_name + ' ' + wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        } else {
            // This is a window that has no role set, and both components of WM_CLASS
            // match (possibly only differing in case), which most likely means either
            // the application doesn't give a damn about distinguishing its various
            // windows, or it's an app that uses role for that, but this window
            // lacks it for some reason. Use non-complete WM_CLASS matching, also
            // include window title in the matching, and pray it causes many more positive
            // matches than negative matches.
            rule->titlematch = Rules::ExactMatch;
            rule->wmclasscomplete = false;
            rule->wmclass = wmclass_class;
            rule->wmclassmatch = Rules::ExactMatch;
        }
    }
    return rule;
}

K_PLUGIN_CLASS_WITH_JSON(KCMKWinRules, "kcm_kwinrules.json");

} // namespace

#include "kcmrules.moc"
