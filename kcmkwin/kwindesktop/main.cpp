/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "main.h"

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusInterface>

#include <KAboutApplicationDialog>
#include <KActionCollection>
#include <KAction>
#include <KCModuleProxy>
#include <KGlobalAccel>
#include <KPluginInfo>
#include <KPluginFactory>
#include <KConfigGroup>
#include <KService>
#include <KServiceTypeTrader>
#include <KShortcutsEditor>

#ifdef Q_WS_X11
#include <QX11Info>
#endif

#include <netwm.h>

K_PLUGIN_FACTORY(KWinDesktopConfigFactory, registerPlugin<KWin::KWinDesktopConfig>();)
K_EXPORT_PLUGIN(KWinDesktopConfigFactory("kcm_kwindesktop"))

namespace KWin
{

KWinDesktopConfigForm::KWinDesktopConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinDesktopConfig::KWinDesktopConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KWinDesktopConfigFactory::componentData(), parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_actionCollection(NULL)
    , m_switchDesktopCollection(NULL)
{
    init();
}

void KWinDesktopConfig::init()
{
    m_ui = new KWinDesktopConfigForm(this);
    // TODO: there has to be a way to add the shortcuts editor to the ui file
    m_editor = new KShortcutsEditor(m_ui, KShortcutsEditor::GlobalAction);
    m_ui->editorFrame->setLayout(new QVBoxLayout());
    m_ui->editorFrame->layout()->setMargin(0);
    m_ui->editorFrame->layout()->addWidget(m_editor);

    m_ui->desktopNames->setDesktopConfig(this);
    m_ui->desktopNames->setMaxDesktops(maxDesktops);
    m_ui->desktopNames->numberChanged(defaultDesktops);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    setQuickHelp(i18n("<h1>Multiple Desktops</h1>In this module, you can configure how many virtual desktops you want and how these should be labeled."));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));
    m_actionCollection->setConfigGroup("Desktop Switching");
    m_actionCollection->setConfigGlobal(true);

    m_switchDesktopCollection = new KActionCollection(this, KComponentData("kwin"));
    m_switchDesktopCollection->setConfigGroup("Desktop Switching");
    m_switchDesktopCollection->setConfigGlobal(true);

    // actions for switch desktop collection - other action is filled dynamically
    KAction* a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch to Next Desktop"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch to Next Desktop"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch to Previous Desktop"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch to Previous Desktop"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch One Desktop to the Right"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch One Desktop to the Right"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch One Desktop to the Left"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch One Desktop to the Left"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch One Desktop Up"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch One Desktop Up"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    a = qobject_cast<KAction*>(m_switchDesktopCollection->addAction("Switch One Desktop Down"));
    a->setProperty("isConfigurationAction", true);
    a->setText(i18n("Switch One Desktop Down"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    m_editor->addCollection(m_switchDesktopCollection, i18n("Desktop Switching"));

#ifdef Q_WS_X11
    // get number of desktops
    NETRootInfo info(QX11Info::display(), NET::NumberOfDesktops | NET::DesktopNames);
    int n = info.numberOfDesktops();

    for (int i = 1; i <= n; ++i) {
        KAction* a = qobject_cast<KAction*>(m_actionCollection->addAction(QString("Switch to Desktop %1").arg(i)));
        a->setProperty("isConfigurationAction", true);
        a->setText(i18n("Switch to Desktop %1", i));
        a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    }

    // This should be after the "Switch to Desktop %1" loop. It HAS to be
    // there after numberSpinBox is connected to slotChangeShortcuts. We would
    // overwrite the users settings if not,
    m_ui->numberSpinBox->setValue(n);

    m_editor->addCollection(m_actionCollection, i18n("Desktop Switching"));
#endif

    // search the effect names
    // TODO: way to recognize if a effect is not found
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString slide;
    QString cube;
    QString fadedesktop;
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_slide'");
    if (!services.isEmpty())
        slide = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cubeslide'");
    if (!services.isEmpty())
        cube = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_fadedesktop'");
    if (!services.isEmpty())
        fadedesktop = services.first()->name();

    m_ui->effectComboBox->addItem(i18n("No Animation"));
    m_ui->effectComboBox->addItem(slide);
    m_ui->effectComboBox->addItem(cube);
    m_ui->effectComboBox->addItem(fadedesktop);

    // effect config and info button
    m_ui->effectInfoButton->setIcon(KIcon("dialog-information"));
    m_ui->effectConfigButton->setIcon(KIcon("configure"));

    connect(m_ui->rowsSpinBox, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->numberSpinBox, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->numberSpinBox, SIGNAL(valueChanged(int)), SLOT(slotChangeShortcuts(int)));
    connect(m_ui->activityCheckBox, SIGNAL(stateChanged(int)), SLOT(changed()));
    connect(m_ui->desktopNames, SIGNAL(changed()), SLOT(changed()));
    connect(m_ui->popupInfoCheckBox, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(m_ui->popupHideSpinBox, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->desktopLayoutIndicatorCheckBox, SIGNAL(stateChanged(int)), SLOT(changed()));
    connect(m_ui->wrapAroundBox, SIGNAL(stateChanged(int)), SLOT(changed()));
    connect(m_editor, SIGNAL(keyChange()), SLOT(changed()));
    connect(m_ui->allShortcutsCheckBox, SIGNAL(stateChanged(int)), SLOT(slotShowAllShortcuts()));
    connect(m_ui->effectComboBox, SIGNAL(currentIndexChanged(int)), SLOT(changed()));
    connect(m_ui->effectComboBox, SIGNAL(currentIndexChanged(int)), SLOT(slotEffectSelectionChanged(int)));
    connect(m_ui->effectInfoButton, SIGNAL(clicked()), SLOT(slotAboutEffectClicked()));
    connect(m_ui->effectConfigButton, SIGNAL(clicked()), SLOT(slotConfigureEffectClicked()));

    // Begin check for immutable - taken from old desktops kcm
#ifdef Q_WS_X11
    int kwin_screen_number = DefaultScreen(QX11Info::display());
#else
    int kwin_screen_number = 0;
#endif

    m_config = KSharedConfig::openConfig("kwinrc");

    QByteArray groupname;
    if (kwin_screen_number == 0)
        groupname = "Desktops";
    else
        groupname = "Desktops-screen-" + QByteArray::number(kwin_screen_number);

    if (m_config->isGroupImmutable(groupname)) {
        m_ui->nameGroup->setEnabled(false);
        //number of desktops widgets
        m_ui->numberLabel->setEnabled(false);
        m_ui->numberSpinBox->setEnabled(false);
        m_ui->rowsSpinBox->setEnabled(false);
    } else {
        KConfigGroup cfgGroup(m_config.data(), groupname.constData());
        if (cfgGroup.isEntryImmutable("Number")) {
            //number of desktops widgets
            m_ui->numberLabel->setEnabled(false);
            m_ui->numberSpinBox->setEnabled(false);
            m_ui->rowsSpinBox->setEnabled(false);
        }
    }
    // End check for immutable
}

KWinDesktopConfig::~KWinDesktopConfig()
{
    undo();
}

void KWinDesktopConfig::defaults()
{
    // TODO: plasma stuff
    m_ui->numberSpinBox->setValue(defaultDesktops);
    m_ui->desktopNames->numberChanged(defaultDesktops);
    for (int i = 1; i <= maxDesktops; i++) {
        m_desktopNames[i-1] = i18n("Desktop %1", i);
        if (i <= defaultDesktops)
            m_ui->desktopNames->setDefaultName(i);
    }

    // popup info
    m_ui->popupInfoCheckBox->setChecked(false);
    m_ui->popupHideSpinBox->setValue(1000);
    m_ui->desktopLayoutIndicatorCheckBox->setChecked(true);

    m_ui->effectComboBox->setCurrentIndex(1);

    m_ui->wrapAroundBox->setChecked(true);

    m_ui->rowsSpinBox->setValue(2);

    m_editor->allDefault();

    emit changed(true);
}


void KWinDesktopConfig::load()
{
    // This method is called on reset(). So undo all changes.
    undo();

#ifdef Q_WS_X11
    // get number of desktops
    unsigned long properties[] = {NET::NumberOfDesktops | NET::DesktopNames, NET::WM2DesktopLayout };
    NETRootInfo info(QX11Info::display(), properties, 2);

    for (int i = 1; i <= maxDesktops; i++) {
        QString name = QString::fromUtf8(info.desktopName(i));
        m_desktopNames << name;
        m_ui->desktopNames->setName(i, name);
    }
    m_ui->rowsSpinBox->setValue(info.desktopLayoutColumnsRows().height());
#endif

    // Popup info
    KConfigGroup effectconfig(m_config, "Plugins");
    KConfigGroup popupInfo(m_config, "Script-desktopchangeosd");
    m_ui->popupInfoCheckBox->setChecked(effectconfig.readEntry("desktopchangeosdEnabled", false));
    m_ui->popupHideSpinBox->setValue(popupInfo.readEntry("PopupHideDelay", 1000));
    m_ui->desktopLayoutIndicatorCheckBox->setChecked(!popupInfo.readEntry("TextOnly", false));

    // Wrap Around on screen edge
    KConfigGroup windowConfig(m_config, "Windows");
    m_ui->wrapAroundBox->setChecked(windowConfig.readEntry<bool>("RollOverDesktops", true));

    // Effect for desktop switching
    // Set current option to "none" if no plugin is activated.
    m_ui->effectComboBox->setCurrentIndex(0);
    if (effectEnabled("slide", effectconfig))
        m_ui->effectComboBox->setCurrentIndex(1);
    if (effectEnabled("cubeslide", effectconfig))
        m_ui->effectComboBox->setCurrentIndex(2);
    if (effectEnabled("fadedesktop", effectconfig))
        m_ui->effectComboBox->setCurrentIndex(3);
    slotEffectSelectionChanged(m_ui->effectComboBox->currentIndex());
    // TODO: plasma stuff

    QDBusInterface interface("org.kde.plasma-desktop", "/App");
    if (interface.isValid()) {
        bool perVirtualDesktopViews = interface.call("perVirtualDesktopViews").arguments().first().toBool();
        m_ui->activityCheckBox->setEnabled(true);
        m_ui->activityCheckBox->setChecked(perVirtualDesktopViews);
    } else
        m_ui->activityCheckBox->setEnabled(false);

    emit changed(false);
}

void KWinDesktopConfig::save()
{
    // TODO: plasma stuff
#ifdef Q_WS_X11
    unsigned long properties[] = {NET::NumberOfDesktops | NET::DesktopNames, NET::WM2DesktopLayout };
    NETRootInfo info(QX11Info::display(), properties, 2);
    // set desktop names
    for (int i = 1; i <= maxDesktops; i++) {
        QString desktopName = m_desktopNames[ i -1 ];
        if (i <= m_ui->numberSpinBox->value())
            desktopName = m_ui->desktopNames->name(i);
        info.setDesktopName(i, desktopName.toUtf8());
        info.activate();
    }
    // set number of desktops
    const int numberDesktops = m_ui->numberSpinBox->value();
    info.setNumberOfDesktops(numberDesktops);
    info.activate();
    int rows =m_ui->rowsSpinBox->value();
    rows = qBound(1, rows, numberDesktops);
    // avoid weird cases like having 3 rows for 4 desktops, where the last row is unused
    int columns = numberDesktops / rows;
    if (numberDesktops % rows > 0) {
        columns++;
    }
    info.setDesktopLayout(NET::OrientationHorizontal, columns, rows, NET::DesktopLayoutCornerTopLeft);
    info.activate();

    XSync(QX11Info::display(), false);

    // save the desktops
    QString groupname;
    const int screenNumber = DefaultScreen(QX11Info::display());
    if (screenNumber == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", screenNumber);
    KConfigGroup group(m_config, groupname);
    group.writeEntry("Rows", rows);
#endif

    // Popup info
    KConfigGroup effectconfig(m_config, "Plugins");
    KConfigGroup popupInfo(m_config, "Script-desktopchangeosd");
    effectconfig.writeEntry("desktopchangeosdEnabled", m_ui->popupInfoCheckBox->isChecked());
    popupInfo.writeEntry("PopupHideDelay", m_ui->popupHideSpinBox->value());
    popupInfo.writeEntry("TextOnly", !m_ui->desktopLayoutIndicatorCheckBox->isChecked());

    // Wrap Around on screen edge
    KConfigGroup windowConfig(m_config, "Windows");
    windowConfig.writeEntry("RollOverDesktops", m_ui->wrapAroundBox->isChecked());

    // Effect desktop switching
    int desktopSwitcher = m_ui->effectComboBox->currentIndex();
    switch(desktopSwitcher) {
    case 0:
        // no effect
        effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
        break;
    case 1:
        // slide
        effectconfig.writeEntry("kwin4_effect_slideEnabled", true);
        effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
        break;
    case 2:
        // cube
        effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", true);
        effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
        break;
    case 3:
        // fadedesktop
        effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
        effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", true);
        break;
    }

    m_editor->save();

    m_config->sync();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    QDBusInterface interface("org.kde.plasma-desktop", "/App");
    interface.call("setPerVirtualDesktopViews", (m_ui->activityCheckBox->isChecked()));

    emit changed(false);
}


void KWinDesktopConfig::undo()
{
    // The global shortcuts editor makes changes active immediately. In case
    // of undo we have to undo them manually
    m_editor->undoChanges();
}

QString KWinDesktopConfig::cachedDesktopName(int desktop)
{
    if (desktop > m_desktopNames.size())
        return QString();
    return m_desktopNames[ desktop -1 ];
}

QString KWinDesktopConfig::extrapolatedShortcut(int desktop) const
{

    if (!desktop || desktop > m_actionCollection->count())
        return QString();
    if (desktop == 1)
        return QString("Ctrl+F1");

    KAction *beforeAction = qobject_cast<KAction*>(m_actionCollection->actions().at(qMin(9, desktop - 2)));
    QString before = beforeAction->globalShortcut(KAction::ActiveShortcut).toString();
    if (before.isEmpty())
        before = beforeAction->globalShortcut(KAction::DefaultShortcut).toString();

    QString seq;
    if (before.contains(QRegExp("F[0-9]{1,2}"))) {
        if (desktop < 13)  // 10?
            seq = QString("F%1").arg(desktop);
        else if (!before.contains("Shift"))
            seq = "Shift+" + QString("F%1").arg(desktop - 10);
    } else if (before.contains(QRegExp("[0-9]"))) {
        if (desktop == 10)
            seq = '0';
        else if (desktop > 10) {
            if (!before.contains("Shift"))
                seq = "Shift+" + QString::number(desktop == 20 ? 0 : (desktop - 10));
        } else
            seq = QString::number(desktop);
    }

    if (!seq.isEmpty()) {
        if (before.contains("Ctrl"))
            seq.prepend("Ctrl+");
        if (before.contains("Alt"))
            seq.prepend("Alt+");
        if (before.contains("Shift"))
            seq.prepend("Shift+");
        if (before.contains("Meta"))
            seq.prepend("Meta+");
    }
    return seq;
}

void KWinDesktopConfig::slotChangeShortcuts(int number)
{
    if ((number < 1) || (number > maxDesktops))
        return;

    if (m_ui->allShortcutsCheckBox->isChecked())
        number = maxDesktops;

    while (number != m_actionCollection->count()) {
        if (number < m_actionCollection->count()) {
            // Remove the action from the action collection. The action itself
            // will still exist because that's the way kwin currently works.
            // No need to remove/forget it. See kwinbindings.
            KAction *a = qobject_cast<KAction*>(
                             m_actionCollection->takeAction(m_actionCollection->actions().last()));
            // Remove any associated global shortcut. Set it to ""
            a->setGlobalShortcut(
                KShortcut(),
                KAction::ActiveShortcut,
                KAction::NoAutoloading);
            m_ui->messageLabel->hide();
            delete a;
        } else {
            // add desktop
            int desktop = m_actionCollection->count() + 1;
            KAction* action = qobject_cast<KAction*>(m_actionCollection->addAction(QString("Switch to Desktop %1").arg(desktop)));
            action->setProperty("isConfigurationAction", true);
            action->setText(i18n("Switch to Desktop %1", desktop));
            action->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
            QString shortcutString = extrapolatedShortcut(desktop);
            if (shortcutString.isEmpty()) {
                m_ui->messageLabel->setText(i18n("No suitable Shortcut for Desktop %1 found", desktop));
                m_ui->messageLabel->show();
            } else {
                KShortcut shortcut(shortcutString);
                if (!shortcut.primary().isEmpty() || KGlobalAccel::self()->isGlobalShortcutAvailable(shortcut.primary())) {
                    action->setGlobalShortcut(shortcut, KAction::ActiveShortcut, KAction::NoAutoloading);
                    m_ui->messageLabel->setText(i18n("Assigned global Shortcut \"%1\" to Desktop %2", shortcutString, desktop));
                    m_ui->messageLabel->show();
                } else {
                    m_ui->messageLabel->setText(i18n("Shortcut conflict: Could not set Shortcut %1 for Desktop %2", shortcutString, desktop));
                    m_ui->messageLabel->show();
                }
            }
        }
    }
    m_editor->clearCollections();
    m_editor->addCollection(m_switchDesktopCollection, i18n("Desktop Switching"));
    m_editor->addCollection(m_actionCollection, i18n("Desktop Switching"));
}

void KWinDesktopConfig::slotShowAllShortcuts()
{
    slotChangeShortcuts(m_ui->numberSpinBox->value());
}

void KWinDesktopConfig::slotEffectSelectionChanged(int index)
{
    bool enabled = false;
    if (index != 0)
        enabled = true;
    m_ui->effectInfoButton->setEnabled(enabled);
    // only cube has config dialog
    if (index != 2)
        enabled = false;
    m_ui->effectConfigButton->setEnabled(enabled);
}


bool KWinDesktopConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

void KWinDesktopConfig::slotAboutEffectClicked()
{
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString effect;
    switch(m_ui->effectComboBox->currentIndex()) {
    case 1:
        effect = "slide";
        break;
    case 2:
        effect = "cubeslide";
        break;
    case 3:
        effect = "fadedesktop";
        break;
    default:
        return;
    }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return;
    KPluginInfo pluginInfo(services.first());

    const QString name    = pluginInfo.name();
    const QString comment = pluginInfo.comment();
    const QString author  = pluginInfo.author();
    const QString email   = pluginInfo.email();
    const QString website = pluginInfo.website();
    const QString version = pluginInfo.version();
    const QString license = pluginInfo.license();
    const QString icon    = pluginInfo.icon();

    KAboutData aboutData(name.toUtf8(), name.toUtf8(), ki18n(name.toUtf8()), version.toUtf8(), ki18n(comment.toUtf8()), KAboutLicense::byKeyword(license).key(), ki18n(QByteArray()), ki18n(QByteArray()), website.toLatin1());
    aboutData.setProgramIconName(icon);
    const QStringList authors = author.split(',');
    const QStringList emails = email.split(',');
    int i = 0;
    if (authors.count() == emails.count()) {
        foreach (const QString & author, authors) {
            if (!author.isEmpty()) {
                aboutData.addAuthor(ki18n(author.toUtf8()), ki18n(QByteArray()), emails[i].toUtf8(), 0);
            }
            i++;
        }
    }
    QPointer<KAboutApplicationDialog> aboutPlugin = new KAboutApplicationDialog(&aboutData, this);
    aboutPlugin->exec();
    delete aboutPlugin;
}

void KWinDesktopConfig::slotConfigureEffectClicked()
{
    QString effect;
    switch(m_ui->effectComboBox->currentIndex()) {
    case 2:
        effect = "cubeslide_config";
        break;
    default:
        return;
    }
    KCModuleProxy* proxy = new KCModuleProxy(effect);
    QPointer< KDialog > configDialog = new KDialog(this);
    configDialog->setWindowTitle(m_ui->effectComboBox->currentText());
    configDialog->setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Default);
    connect(configDialog, SIGNAL(defaultClicked()), proxy, SLOT(defaults()));

    QWidget *showWidget = new QWidget(configDialog);
    QVBoxLayout *layout = new QVBoxLayout;
    showWidget->setLayout(layout);
    layout->addWidget(proxy);
    layout->insertSpacing(-1, KDialog::marginHint());
    configDialog->setMainWidget(showWidget);

    if (configDialog->exec() == QDialog::Accepted) {
        proxy->save();
    } else {
        proxy->load();
    }
    delete configDialog;
}

} // namespace

#include "main.moc"
