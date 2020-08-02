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

    connect(m_rulesModel, &RulesModel::descriptionChanged, this, [this]{
        if (m_editIndex.isValid()) {
            m_ruleBookModel->setDescriptionAt(m_editIndex.row(), m_rulesModel->description());
        }
    } );
    connect(m_rulesModel, &RulesModel::dataChanged, this, &KCMKWinRules::updateNeedsSave);
    connect(m_ruleBookModel, &RulesModel::dataChanged, this, &KCMKWinRules::updateNeedsSave);
}

void KCMKWinRules::load()
{
    m_ruleBookModel->load();

    m_editIndex = QModelIndex();
    emit editIndexChanged();

    setNeedsSave(false);
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

K_PLUGIN_CLASS_WITH_JSON(KCMKWinRules, "kcm_kwinrules.json");

} // namespace

#include "kcmrules.moc"
