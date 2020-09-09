/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main.h"
#include <effect_builtins.h>
#include <kwin_effects_interface.h>

// Qt
#include <QtDBus>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QTabWidget>
#include <QStandardPaths>
#include <QPointer>
#include <QStandardItemModel>

// KDE
#include <KCModuleProxy>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginTrader>
#include <KTitleWidget>
#include <KServiceTypeTrader>
#include <KNewStuff3/KNS3/DownloadDialog>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>

// own
#include "kwintabboxconfigform.h"
#include "layoutpreview.h"
#include "kwintabboxsettings.h"
#include "kwinswitcheffectsettings.h"
#include "kwinpluginssettings.h"

K_PLUGIN_FACTORY(KWinTabBoxConfigFactory, registerPlugin<KWin::KWinTabBoxConfig>();)

namespace KWin
{

using namespace TabBox;


KWinTabBoxConfig::KWinTabBoxConfig(QWidget* parent, const QVariantList& args)
    : KCModule(parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_tabBoxConfig(new TabBoxSettings(QStringLiteral("TabBox"), this))
    , m_tabBoxAlternativeConfig(new TabBoxSettings(QStringLiteral("TabBoxAlternative"), this))
    , m_coverSwitchConfig(new SwitchEffectSettings(QStringLiteral("Effect-CoverSwitch"), this))
    , m_flipSwitchConfig(new SwitchEffectSettings(QStringLiteral("Effect-FlipSwitch"), this))
    , m_pluginsConfig(new PluginsSettings(this))
{
    QTabWidget* tabWidget = new QTabWidget(this);
    m_primaryTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Main, tabWidget);
    m_alternativeTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Alternative, tabWidget);
    tabWidget->addTab(m_primaryTabBoxUi, i18n("Main"));
    tabWidget->addTab(m_alternativeTabBoxUi, i18n("Alternative"));

    QPushButton* ghnsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("get-hot-new-stuff")), i18n("Get New Task Switchers..."));
    connect(ghnsButton, SIGNAL(clicked(bool)), SLOT(slotGHNS()));

    QHBoxLayout* buttonBar = new QHBoxLayout();
    QSpacerItem* buttonBarSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonBar->addItem(buttonBarSpacer);
    buttonBar->addWidget(ghnsButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    KTitleWidget* infoLabel = new KTitleWidget(tabWidget);
    infoLabel->setText(i18n("Focus policy settings limit the functionality of navigating through windows."),
                       KTitleWidget::InfoMessage);
    infoLabel->setIcon(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
    layout->addWidget(infoLabel,0);
    layout->addWidget(tabWidget,1);
    layout->addLayout(buttonBar);
    setLayout(layout);

    addConfig(m_tabBoxConfig, m_primaryTabBoxUi);
    addConfig(m_tabBoxAlternativeConfig, m_alternativeTabBoxUi);

    createConnections(m_primaryTabBoxUi);
    createConnections(m_alternativeTabBoxUi);

    initLayoutLists();

    // check focus policy - we don't offer configs for unreasonable focus policies
    KConfigGroup config(m_config, "Windows");
    QString policy = config.readEntry("FocusPolicy", "ClickToFocus");
    if ((policy == "FocusUnderMouse") || (policy == "FocusStrictlyUnderMouse")) {
        tabWidget->setEnabled(false);
        infoLabel->show();
    } else {
        infoLabel->hide();
    }

    setEnabledUi(m_primaryTabBoxUi, m_tabBoxConfig);
    setEnabledUi(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);
}

KWinTabBoxConfig::~KWinTabBoxConfig()
{
}

static QList<KPackage::Package> availableLnFPackages()
{
    QList<KPackage::Package> packages;
    QStringList paths;
    const QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    for (const QString &path : dataPaths) {
        QDir dir(path + QLatin1String("/plasma/look-and-feel"));
        paths << dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    const auto &p = paths;
    for (const QString &path : p) {
        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
        pkg.setPath(path);
        pkg.setFallbackPackage(KPackage::Package());
        if (!pkg.filePath("defaults").isEmpty()) {
            KSharedConfigPtr conf = KSharedConfig::openConfig(pkg.filePath("defaults"));
            KConfigGroup cg = KConfigGroup(conf, "kwinrc");
            cg = KConfigGroup(&cg, "WindowSwitcher");
            if (!cg.readEntry("LayoutName", QString()).isEmpty()) {
                packages << pkg;
            }
        }
    }

    return packages;
}

void KWinTabBoxConfig::initLayoutLists()
{
    // search the effect names
    m_coverSwitch = BuiltInEffects::effectData(BuiltInEffect::CoverSwitch).name;
    m_flipSwitch = BuiltInEffects::effectData(BuiltInEffect::FlipSwitch).name;

    QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->listPackages("KWin/WindowSwitcher");
    QStringList layoutNames, layoutPlugins, layoutPaths;

    const auto lnfPackages = availableLnFPackages();
    for (const auto &package : lnfPackages) {
        const auto &metaData = package.metadata();
        layoutNames << metaData.name();
        layoutPlugins << metaData.pluginId();
        layoutPaths << package.filePath("windowswitcher", QStringLiteral("WindowSwitcher.qml"));
    }

    for (const auto &offer : offers) {
        const QString pluginName = offer.pluginId();
        if (offer.value("X-Plasma-API") != "declarativeappletscript") {
            continue;
        }
        //we don't have a proper servicetype
        if (offer.value("X-KWin-Exclude-Listing") == QStringLiteral("true")) {
            continue;
        }
        const QString scriptName = offer.value("X-Plasma-MainScript");
        const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                          QLatin1String("kwin/tabbox/") + pluginName + QLatin1String("/contents/")
                                                          + scriptName);
        if (scriptFile.isNull()) {
            continue;
        }

        layoutNames << offer.name();
        layoutPlugins << pluginName;
        layoutPaths << scriptFile;
    }


    KWinTabBoxConfigForm *ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi };
    for (int i=0; i<2; ++i) {
        QStandardItemModel *model = new QStandardItemModel;

        QStandardItem *coverItem = new QStandardItem(BuiltInEffects::effectData(BuiltInEffect::CoverSwitch).displayName);
        coverItem->setData(m_coverSwitch, Qt::UserRole);
        coverItem->setData(false, KWinTabBoxConfigForm::AddonEffect);
        model->appendRow(coverItem);

        QStandardItem *flipItem = new QStandardItem(BuiltInEffects::effectData(BuiltInEffect::FlipSwitch).displayName);
        flipItem->setData(m_flipSwitch, Qt::UserRole);
        flipItem->setData(false, KWinTabBoxConfigForm::AddonEffect);
        model->appendRow(flipItem);

        for (int j = 0; j < layoutNames.count(); ++j) {
            QStandardItem *item = new QStandardItem(layoutNames[j]);
            item->setData(layoutPlugins[j], Qt::UserRole);
            item->setData(layoutPaths[j], KWinTabBoxConfigForm::LayoutPath);
            item->setData(true, KWinTabBoxConfigForm::AddonEffect);
            model->appendRow(item);
        }
        model->sort(0);
        ui[i]->setEffectComboModel(model);
    }
}

void KWinTabBoxConfig::setEnabledUi(KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    form->setHighlightWindowsEnabled(!config->isHighlightWindowsImmutable());
    form->setFilterScreenEnabled(!config->isMultiScreenModeImmutable());
    form->setFilterDesktopEnabled(!config->isDesktopModeImmutable());
    form->setFilterActivitiesEnabled(!config->isActivitiesModeImmutable());
    form->setFilterMinimizationEnabled(!config->isMinimizedModeImmutable());
    form->setApplicationModeEnabled(!config->isApplicationsModeImmutable());
    form->setShowDesktopModeEnabled(!config->isShowDesktopModeImmutable());
    form->setSwitchingModeEnabled(!config->isSwitchingModeImmutable());
    form->setLayoutNameEnabled(!config->isLayoutNameImmutable());
}

void KWinTabBoxConfig::createConnections(KWinTabBoxConfigForm *form)
{
    connect(form, SIGNAL(effectConfigButtonClicked()), this, SLOT(configureEffectClicked()));

    connect(form, SIGNAL(filterScreenChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(filterDesktopChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(filterActivitiesChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(filterMinimizationChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(applicationModeChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(showDesktopModeChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(switchingModeChanged(int)), this, SLOT(updateUnmanagedState()));
    connect(form, SIGNAL(layoutNameChanged(QString)), this, SLOT(updateUnmanagedState()));
}

void KWinTabBoxConfig::updateUnmanagedState()
{
    bool isNeedSave = false;
    isNeedSave |= updateUnmanagedIsNeedSave(m_primaryTabBoxUi, m_tabBoxConfig);
    isNeedSave |= updateUnmanagedIsNeedSave(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);

    unmanagedWidgetChangeState(isNeedSave);

    bool isDefault = true;
    isDefault &= updateUnmanagedIsDefault(m_primaryTabBoxUi, m_tabBoxConfig);
    isDefault &= updateUnmanagedIsDefault(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);

    unmanagedWidgetDefaultState(isDefault);
}

bool KWinTabBoxConfig::updateUnmanagedIsNeedSave(const KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    bool isNeedSave = false;
    isNeedSave |= form->filterScreen() != config->multiScreenMode();
    isNeedSave |= form->filterDesktop() != config->desktopMode();
    isNeedSave |= form->filterActivities() != config->activitiesMode();
    isNeedSave |= form->filterMinimization() != config->minimizedMode();
    isNeedSave |= form->applicationMode() != config->applicationsMode();
    isNeedSave |= form->showDesktopMode() != config->showDesktopMode();
    isNeedSave |= form->switchingMode() != config->switchingMode();
    isNeedSave |= form->layoutName() != config->layoutName();

    return isNeedSave;
}

bool KWinTabBoxConfig::updateUnmanagedIsDefault(const KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    bool isDefault = true;
    isDefault &= form->filterScreen() == config->defaultMultiScreenModeValue();
    isDefault &= form->filterDesktop() == config->defaultDesktopModeValue();
    isDefault &= form->filterActivities() == config->defaultActivitiesModeValue();
    isDefault &= form->filterMinimization() == config->defaultMinimizedModeValue();
    isDefault &= form->applicationMode() == config->defaultApplicationsModeValue();
    isDefault &= form->showDesktopMode() == config->defaultShowDesktopModeValue();
    isDefault &= form->switchingMode() == config->defaultSwitchingModeValue();
    isDefault &= form->layoutName() == config->defaultLayoutNameValue();

    return isDefault;
}

void KWinTabBoxConfig::load()
{
    KCModule::load();

    m_tabBoxConfig->load();
    m_tabBoxAlternativeConfig->load();

    updateUiFromConfig(m_primaryTabBoxUi, m_tabBoxConfig);
    updateUiFromConfig(m_alternativeTabBoxUi , m_tabBoxAlternativeConfig);

    m_coverSwitchConfig->load();
    m_flipSwitchConfig->load();

    m_pluginsConfig->load();

    if (m_pluginsConfig->coverswitchEnabled()) {
        if (m_coverSwitchConfig->tabBox()) {
            m_primaryTabBoxUi->setLayoutName(m_coverSwitch);
        }
        if (m_coverSwitchConfig->tabBoxAlternative()) {
            m_alternativeTabBoxUi->setLayoutName(m_coverSwitch);
        }
    }
    if (m_pluginsConfig->flipswitchEnabled()) {
        if (m_flipSwitchConfig->tabBox()) {
            m_primaryTabBoxUi->setLayoutName(m_flipSwitch);
        }
        if (m_flipSwitchConfig->tabBoxAlternative()) {
            m_alternativeTabBoxUi->setLayoutName(m_flipSwitch);
        }
    }

    m_primaryTabBoxUi->loadShortcuts();
    m_alternativeTabBoxUi->loadShortcuts();

    updateUnmanagedState();
}

void KWinTabBoxConfig::save()
{
    // effects
    const bool highlightWindows = m_primaryTabBoxUi->highlightWindows() || m_alternativeTabBoxUi->highlightWindows();
    const bool coverSwitch = m_primaryTabBoxUi->showTabBox()
            && m_primaryTabBoxUi->effectComboCurrentData().toString() == m_coverSwitch;
    const bool flipSwitch = m_primaryTabBoxUi->showTabBox()
            && m_primaryTabBoxUi->effectComboCurrentData().toString() == m_flipSwitch;
    const bool coverSwitchAlternative = m_alternativeTabBoxUi->showTabBox()
            && m_alternativeTabBoxUi->effectComboCurrentData().toString() == m_coverSwitch;
    const bool flipSwitchAlternative = m_alternativeTabBoxUi->showTabBox()
            && m_alternativeTabBoxUi->effectComboCurrentData().toString() == m_flipSwitch;

    // activate effects if not active
    if (coverSwitch || coverSwitchAlternative) {
        m_pluginsConfig->setCoverswitchEnabled(true);
    }
    if (flipSwitch || flipSwitchAlternative) {
        m_pluginsConfig->setFlipswitchEnabled(true);
    }
    if (highlightWindows) {
        m_pluginsConfig->setHighlightwindowEnabled(true);
    }
    m_pluginsConfig->save();

    m_coverSwitchConfig->setTabBox(coverSwitch);
    m_coverSwitchConfig->setTabBoxAlternative(coverSwitchAlternative);
    m_coverSwitchConfig->save();

    m_flipSwitchConfig->setTabBox(flipSwitch);
    m_flipSwitchConfig->setTabBoxAlternative(flipSwitchAlternative);
    m_flipSwitchConfig->save();

    updateConfigFromUi(m_primaryTabBoxUi, m_tabBoxConfig);
    updateConfigFromUi(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);

    m_tabBoxConfig->save();
    m_tabBoxAlternativeConfig->save();

    KCModule::save();
    updateUnmanagedState();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
    // and reconfigure the effects
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Effects"),
                                             QDBusConnection::sessionBus());
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::CoverSwitch));
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::FlipSwitch));
}

void KWinTabBoxConfig::defaults()
{
    m_coverSwitchConfig->setDefaults();
    m_flipSwitchConfig->setDefaults();

    updateUiFromDefaultConfig(m_primaryTabBoxUi, m_tabBoxConfig);
    updateUiFromDefaultConfig(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);

    m_primaryTabBoxUi->resetShortcuts();
    m_alternativeTabBoxUi->resetShortcuts();

    KCModule::defaults();
    updateUnmanagedState();
}

void KWinTabBoxConfig::updateUiFromConfig(KWinTabBoxConfigForm *form, const KWin::TabBox::TabBoxSettings *config)
{
    form->setFilterScreen(static_cast<TabBoxConfig::ClientMultiScreenMode>(config->multiScreenMode()));
    form->setFilterDesktop(static_cast<TabBoxConfig::ClientDesktopMode>(config->desktopMode()));
    form->setFilterActivities(static_cast<TabBoxConfig::ClientActivitiesMode>(config->activitiesMode()));
    form->setFilterMinimization(static_cast<TabBoxConfig::ClientMinimizedMode>(config->minimizedMode()));
    form->setApplicationMode(static_cast<TabBoxConfig::ClientApplicationsMode>(config->applicationsMode()));
    form->setShowDesktopMode(static_cast<TabBoxConfig::ShowDesktopMode>(config->showDesktopMode()));
    form->setSwitchingModeChanged(static_cast<TabBoxConfig::ClientSwitchingMode>(config->switchingMode()));
    form->setLayoutName(config->layoutName());
}

void KWinTabBoxConfig::updateConfigFromUi(const KWinTabBoxConfigForm *form, TabBoxSettings *config)
{
    config->setMultiScreenMode(form->filterScreen());
    config->setDesktopMode(form->filterDesktop());
    config->setActivitiesMode(form->filterActivities());
    config->setMinimizedMode(form->filterMinimization());
    config->setApplicationsMode(form->applicationMode());
    config->setShowDesktopMode(form->showDesktopMode());
    config->setSwitchingMode(form->switchingMode());
    config->setLayoutName(form->layoutName());
}

void KWinTabBoxConfig::updateUiFromDefaultConfig(KWinTabBoxConfigForm *form, const KWin::TabBox::TabBoxSettings *config)
{
    form->setFilterScreen(static_cast<TabBoxConfig::ClientMultiScreenMode>(config->defaultMultiScreenModeValue()));
    form->setFilterDesktop(static_cast<TabBoxConfig::ClientDesktopMode>(config->defaultDesktopModeValue()));
    form->setFilterActivities(static_cast<TabBoxConfig::ClientActivitiesMode>(config->defaultActivitiesModeValue()));
    form->setFilterMinimization(static_cast<TabBoxConfig::ClientMinimizedMode>(config->defaultMinimizedModeValue()));
    form->setApplicationMode(static_cast<TabBoxConfig::ClientApplicationsMode>(config->defaultApplicationsModeValue()));
    form->setShowDesktopMode(static_cast<TabBoxConfig::ShowDesktopMode>(config->defaultShowDesktopModeValue()));
    form->setSwitchingModeChanged(static_cast<TabBoxConfig::ClientSwitchingMode>(config->defaultSwitchingModeValue()));
    form->setLayoutName(config->defaultLayoutNameValue());
}

void KWinTabBoxConfig::configureEffectClicked()
{
    auto form = qobject_cast<KWinTabBoxConfigForm *>(sender());
    Q_ASSERT(form);

    if (form->effectComboCurrentData(KWinTabBoxConfigForm::AddonEffect).toBool()) {
        // Show the preview for addon effect
        new LayoutPreview(form->effectComboCurrentData(KWinTabBoxConfigForm::LayoutPath).toString(), this);
    } else {
        // For builtin effect, display a configuration dialog
        QPointer<QDialog> configDialog = new QDialog(this);
        configDialog->setLayout(new QVBoxLayout);
        configDialog->setWindowTitle(form->effectComboCurrentData(Qt::DisplayRole).toString());
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults, configDialog);
        connect(buttonBox, SIGNAL(accepted()), configDialog, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), configDialog, SLOT(reject()));

        const QString name = form->effectComboCurrentData().toString();
        KCModule *kcm = KPluginTrader::createInstanceFromQuery<KCModule>(QStringLiteral("kwin/effects/configs/"), QString(),
                                                                         QStringLiteral("'%1' in [X-KDE-ParentComponents]").arg(name),
                                                                         configDialog);
        if (!kcm) {
            delete configDialog;
            return;
        }

        connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, kcm, &KCModule::defaults);

        QWidget *showWidget = new QWidget(configDialog);
        QVBoxLayout *layout = new QVBoxLayout;
        showWidget->setLayout(layout);
        layout->addWidget(kcm);
        configDialog->layout()->addWidget(showWidget);
        configDialog->layout()->addWidget(buttonBox);

        if (configDialog->exec() == QDialog::Accepted) {
            kcm->save();
        } else {
            kcm->load();
        }
        delete configDialog;
    }
}

void KWinTabBoxConfig::slotGHNS()
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog("kwinswitcher.knsrc", this);
    if (downloadDialog->exec() == QDialog::Accepted) {
        if (!downloadDialog->changedEntries().isEmpty()) {
            initLayoutLists();
        }
    }
    delete downloadDialog;
}

} // namespace

#include "main.moc"
