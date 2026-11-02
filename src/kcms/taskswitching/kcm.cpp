/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QWindow>
#include <qqml.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KShortcutsDialog>

#include "tabbox/tabboxconfig.h"

#include "tabboxlayoutsmodel.h"
#include "taskswitchingdata.h"

#include "kcm.h"

using namespace Qt::StringLiterals;

K_PLUGIN_FACTORY_WITH_JSON(TaskSwitchingKCMFactory, "kcm_kwin_taskswitching.json", registerPlugin<KWin::TaskSwitchingKCM>(); registerPlugin<KWin::TaskSwitchingData>();)

namespace KWin
{

TaskSwitchingKCM::TaskSwitchingKCM(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_data(new TaskSwitchingData(this))
    , m_tabBoxLayouts(new TabBoxLayoutsModel(this))
{
    qmlRegisterAnonymousType<TabBoxSettings>("org.kde.kwin.kcmtaskswitching", 0);
    qmlRegisterAnonymousType<OverviewSettings>("org.kde.kwin.kcmtaskswitching", 0);

    qmlRegisterUncreatableType<TabBoxLayoutsModel>("org.kde.kwin.kcmtaskswitching", 0, 0, "TabBoxLayoutsModel", "Role enums only");
    qmlRegisterUncreatableType<TabBox::TabBoxConfig>("org.kde.kwin.kcmtaskswitching", 0, 0, "tabBoxConfig", "Settings enums only");

    setButtons(Apply | Default | Help);
}

TaskSwitchingKCM::~TaskSwitchingKCM()
{
}

TabBoxSettings *TaskSwitchingKCM::tabBoxSettings() const
{
    return m_data->tabBoxSettings();
}

TabBoxSettings *TaskSwitchingKCM::tabBoxAlternativeSettings() const
{
    return m_data->tabBoxAlternativeSettings();
}

TabBoxLayoutsModel *TaskSwitchingKCM::tabBoxLayouts() const
{
    return m_tabBoxLayouts;
}

OverviewSettings *TaskSwitchingKCM::overviewSettings() const
{
    return m_data->overviewSettings();
}

bool TaskSwitchingKCM::isDefaults() const
{
    return m_data->isDefaults();
}

void TaskSwitchingKCM::save()
{
    const bool tabBoxSaveNeeded = std::any_of(tabBoxSettings()->items().cbegin(), tabBoxSettings()->items().cend(), [](KConfigSkeletonItem *item) {
        return item->isSaveNeeded();
    }) || std::any_of(tabBoxAlternativeSettings()->items().cbegin(), tabBoxAlternativeSettings()->items().cend(), [](KConfigSkeletonItem *item) {
        return item->isSaveNeeded();
    });

    const bool overviewSaveNeeded = overviewSettings()->findItem(u"Overview"_s)->isSaveNeeded();
    const bool overviewSettingsSaveNeeded = overviewSettings()->findItem(u"IgnoreMinimized"_s)->isSaveNeeded()
        || overviewSettings()->findItem(u"FilterWindows"_s)->isSaveNeeded()
        || overviewSettings()->findItem(u"OrganizedGrid"_s)->isSaveNeeded();

    KQuickManagedConfigModule::save();

    if (tabBoxSaveNeeded) {
        QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }

    if (overviewSaveNeeded) {
        QDBusMessage reloadMessage =
            QDBusMessage::createMethodCall(u"org.kde.KWin"_s,
                                           u"/Effects"_s,
                                           u"org.kde.kwin.Effects"_s,
                                           overviewSettings()->overview() ? u"loadEffect"_s : u"unloadEffect"_s);
        reloadMessage.setArguments({u"overview"_s});
        QDBusConnection::sessionBus().call(reloadMessage);
    }

    if (overviewSettingsSaveNeeded && overviewSettings()->overview()) {
        QDBusMessage reconfigureMessage = QDBusMessage::createMethodCall(u"org.kde.KWin"_s,
                                                                         u"/Effects"_s,
                                                                         u"org.kde.kwin.Effects"_s,
                                                                         u"reconfigureEffect"_s);
        reconfigureMessage.setArguments({u"overview"_s});
        QDBusConnection::sessionBus().call(reconfigureMessage);
    }
}

void TaskSwitchingKCM::configureTabBoxShortcuts(const bool showAlternative)
{
    KShortcutsDialog *dialog = new KShortcutsDialog(KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    KActionCollection *actionCollection = new KActionCollection(dialog, u"kwin"_s);
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a;
    KLocalizedString name;
    if (!showAlternative) {
        name = ki18ndc("kwin", "@action", "Walk Through Windows");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_Tab, Qt::ALT | Qt::Key_Tab});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_Tab, Qt::ALT | Qt::Key_Tab});

        name = ki18ndc("kwin", "@action", "Walk Through Windows (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::SHIFT | Qt::Key_Tab, Qt::ALT | Qt::SHIFT | Qt::Key_Tab});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::SHIFT | Qt::Key_Tab, Qt::ALT | Qt::SHIFT | Qt::Key_Tab});

        name = ki18ndc("kwin", "@action", "Walk Through Windows of Current Application");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_QuoteLeft, Qt::ALT | Qt::Key_QuoteLeft});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_QuoteLeft, Qt::ALT | Qt::Key_QuoteLeft});

        name = ki18ndc("kwin", "@action", "Walk Through Windows of Current Application (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_AsciiTilde, Qt::ALT | Qt::Key_AsciiTilde});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_AsciiTilde, Qt::ALT | Qt::Key_AsciiTilde});
    } else {
        name = ki18ndc("kwin", "@action", "Walk Through Windows Alternative");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18ndc("kwin", "@action", "Walk Through Windows Alternative (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18ndc("kwin", "@action", "Walk Through Windows of Current Application Alternative");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18ndc("kwin", "@action", "Walk Through Windows of Current Application Alternative (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});
    }

    dialog->addCollection(actionCollection);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(QGuiApplication::focusWindow());
    dialog->setWindowModality(Qt::WindowModal);

    dialog->configure();
}

void TaskSwitchingKCM::configureOverviewShortcuts()
{
    KShortcutsDialog *dialog = new KShortcutsDialog(KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    KActionCollection *actionCollection = new KActionCollection(dialog, u"kwin"_s);
    actionCollection->setComponentDisplayName(i18nc("@title", "KWin"));

    QAction *a;

    a = actionCollection->addAction(u"Cycle Overview"_s);
    a->setText(i18nc("@action Overview and Grid View are the name of KWin effects", "Cycle through Overview and Grid View"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {});
    KGlobalAccel::self()->setShortcut(a, {});

    a = actionCollection->addAction(u"Cycle Overview Opposite"_s);
    a->setText(i18nc("@action Grid View and Overview are the name of KWin effects", "Cycle through Grid View and Overview"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {});
    KGlobalAccel::self()->setShortcut(a, {});

    a = actionCollection->addAction(u"Overview"_s);
    a->setText(i18nc("@action Overview is the name of a KWin effect", "Toggle Overview"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_W});
    KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_W});

    a = actionCollection->addAction(u"Grid View"_s);
    a->setText(i18nc("@action Grid View is the name of a KWin effect", "Toggle Grid View"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_G});
    KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_G});

    dialog->addCollection(actionCollection);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(QGuiApplication::focusWindow());
    dialog->setWindowModality(Qt::WindowModal);

    dialog->configure();
}

void TaskSwitchingKCM::ghnsEntryChanged()
{
    m_tabBoxLayouts->load();
}

void TaskSwitchingKCM::showPreview(const QString &pluginId, bool showDesktop)
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/TabBox"_s, u"org.kde.kwin.TabBox"_s, u"preview"_s);
    message.setArguments({pluginId, showDesktop});
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

} // namespace Kwin

#include "kcm.moc"

#include "moc_kcm.cpp"
