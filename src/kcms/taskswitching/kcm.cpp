/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
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

bool TaskSwitchingKCM::isDefaults() const
{
    return m_data->isDefaults();
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

} // namespace Kwin

#include "kcm.moc"

#include "moc_kcm.cpp"
