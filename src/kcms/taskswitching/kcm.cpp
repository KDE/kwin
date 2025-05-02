/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QWindow>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KShortcutsDialog>

#include "config-kwin.h"
#include "tabbox/tabboxconfig.h"

#include "tabboxlayoutsmodel.h"
#include "taskswitchingdata.h"

#include "kcm.h"

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

    const bool overviewSaveNeeded = overviewSettings()->findItem(QStringLiteral("Overview"))->isSaveNeeded();
    const bool overviewSettingsSaveNeeded = overviewSettings()->findItem(QStringLiteral("IgnoreMinimized"))->isSaveNeeded()
        || overviewSettings()->findItem(QStringLiteral("FilterWindows"))->isSaveNeeded()
        || overviewSettings()->findItem(QStringLiteral("OrganizedGrid"))->isSaveNeeded();

    KQuickManagedConfigModule::save();

    if (tabBoxSaveNeeded) {
        QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }

    if (overviewSaveNeeded) {
        QDBusMessage reloadMessage =
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                           QStringLiteral("/Effects"),
                                           QStringLiteral("org.kde.kwin.Effects"),
                                           overviewSettings()->overview() ? QStringLiteral("loadEffect") : QStringLiteral("unloadEffect"));
        reloadMessage.setArguments({QStringLiteral("overview")});
        QDBusConnection::sessionBus().call(reloadMessage);
    }

    if (overviewSettingsSaveNeeded && overviewSettings()->overview()) {
        QDBusMessage reconfigureMessage = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                                         QStringLiteral("/Effects"),
                                                                         QStringLiteral("org.kde.kwin.Effects"),
                                                                         QStringLiteral("reconfigureEffect"));
        reconfigureMessage.setArguments({QStringLiteral("overview")});
        QDBusConnection::sessionBus().call(reconfigureMessage);
    }
}

void TaskSwitchingKCM::previewTabBoxLayout(const QString &path, const bool showDesktopMode)
{
    // The process will close when losing focus, but check in case of multiple calls
    if (m_tabBoxLayoutPreviewProcess && m_tabBoxLayoutPreviewProcess->state() != QProcess::NotRunning) {
        return;
    }

    // Launch the preview helper executable with the required env var
    // that allows the PlasmaDialog to position itself
    // QT_WAYLAND_DISABLE_FIXED_POSITIONS=1 kwin-tabbox-preview <path> [--show-desktop]

    const QString previewHelper = QStandardPaths::findExecutable("kwin-tabbox-preview", {LIBEXEC_DIR});
    if (previewHelper.isEmpty()) {
        qWarning() << "Cannot find tabbox preview helper executable \"kwin-tabbox-preview\" in" << LIBEXEC_DIR;
        return;
    }

    QStringList args;
    args << path;
    if (showDesktopMode) {
        args << QStringLiteral("--show-desktop");
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_FIXED_POSITIONS"),
               QStringLiteral("1"));

    m_tabBoxLayoutPreviewProcess = std::make_unique<QProcess>();
    m_tabBoxLayoutPreviewProcess->setArguments(args);
    m_tabBoxLayoutPreviewProcess->setProgram(previewHelper);
    m_tabBoxLayoutPreviewProcess->setProcessEnvironment(env);
    m_tabBoxLayoutPreviewProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_tabBoxLayoutPreviewProcess->start();
}

void TaskSwitchingKCM::configureTabBoxShortcuts(const bool showAlternative)
{
    KShortcutsDialog *dialog = new KShortcutsDialog(KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    KActionCollection *actionCollection = new KActionCollection(dialog, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a;
    KLocalizedString name;
    if (!showAlternative) {
        name = ki18nd("kwin", "Walk Through Windows");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_Tab, Qt::ALT | Qt::Key_Tab});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_Tab, Qt::ALT | Qt::Key_Tab});

        name = ki18nd("kwin", "Walk Through Windows (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::SHIFT | Qt::Key_Tab, Qt::ALT | Qt::SHIFT | Qt::Key_Tab});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::SHIFT | Qt::Key_Tab, Qt::ALT | Qt::SHIFT | Qt::Key_Tab});

        name = ki18nd("kwin", "Walk Through Windows of Current Application");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_QuoteLeft, Qt::ALT | Qt::Key_QuoteLeft});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_QuoteLeft, Qt::ALT | Qt::Key_QuoteLeft});

        name = ki18nd("kwin", "Walk Through Windows of Current Application (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_AsciiTilde, Qt::ALT | Qt::Key_AsciiTilde});
        KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_AsciiTilde, Qt::ALT | Qt::Key_AsciiTilde});
    } else {
        name = ki18nd("kwin", "Walk Through Windows Alternative");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18nd("kwin", "Walk Through Windows Alternative (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18nd("kwin", "Walk Through Windows of Current Application Alternative");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});

        name = ki18nd("kwin", "Walk Through Windows of Current Application Alternative (Reverse)");
        a = actionCollection->addAction(name.untranslatedText());
        a->setText(name.toString());
        a->setProperty("isConfigurationAction", true);
        KGlobalAccel::self()->setDefaultShortcut(a, {});
        KGlobalAccel::self()->setShortcut(a, {});
    }

    dialog->addCollection(actionCollection);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(QApplication::activeWindow()->windowHandle());
    dialog->setWindowModality(Qt::WindowModal);

    dialog->configure();
}

void TaskSwitchingKCM::configureOverviewShortcuts()
{
    KShortcutsDialog *dialog = new KShortcutsDialog(KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    KActionCollection *actionCollection = new KActionCollection(dialog, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a;
    KLocalizedString name;

    a = actionCollection->addAction(QStringLiteral("Cycle Overview"));
    a->setText(i18nc("@action Overview and Grid View are the name of KWin effects", "Cycle through Overview and Grid View"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {});
    KGlobalAccel::self()->setShortcut(a, {});

    a = actionCollection->addAction(QStringLiteral("Cycle Overview Opposite"));
    a->setText(i18nc("@action Grid View and Overview are the name of KWin effects", "Cycle through Grid View and Overview"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {});
    KGlobalAccel::self()->setShortcut(a, {});

    a = actionCollection->addAction(QStringLiteral("Overview"));
    a->setText(i18nc("@action Overview is the name of a KWin effect", "Toggle Overview"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_W});
    KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_W});

    a = actionCollection->addAction(QStringLiteral("Grid View"));
    a->setText(i18nc("@action Grid View is the name of a KWin effect", "Toggle Grid View"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, {Qt::META | Qt::Key_G});
    KGlobalAccel::self()->setShortcut(a, {Qt::META | Qt::Key_G});

    dialog->addCollection(actionCollection);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(QApplication::activeWindow()->windowHandle());
    dialog->setWindowModality(Qt::WindowModal);

    dialog->configure();
}

void TaskSwitchingKCM::ghnsEntryChanged()
{
    m_tabBoxLayouts->load();
}

} // namespace Kwin

#include "kcm.moc"

#include "moc_kcm.cpp"
