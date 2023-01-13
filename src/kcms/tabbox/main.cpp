/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main.h"
#include <kwin_effects_interface.h>

// Qt
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QSpacerItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtDBus>

// KDE
#include <KLocalizedString>
#include <KNSWidgets/Button>
#include <KPluginFactory>
#include <KTitleWidget>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>

// own
#include "kwinpluginssettings.h"
#include "kwinswitcheffectsettings.h"
#include "kwintabboxconfigform.h"
#include "kwintabboxdata.h"
#include "kwintabboxsettings.h"
#include "layoutpreview.h"

K_PLUGIN_FACTORY_WITH_JSON(KWinTabBoxConfigFactory, "kcm_kwintabbox.json", registerPlugin<KWin::KWinTabBoxConfig>(); registerPlugin<KWin::TabBox::KWinTabboxData>();)

namespace KWin
{

using namespace TabBox;

KWinTabBoxConfig::KWinTabBoxConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_data(new KWinTabboxData(this))
{
    QTabWidget *tabWidget = new QTabWidget(this);
    m_primaryTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Main, tabWidget);
    m_alternativeTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Alternative, tabWidget);
    tabWidget->addTab(m_primaryTabBoxUi, i18n("Main"));
    tabWidget->addTab(m_alternativeTabBoxUi, i18n("Alternative"));

    KNSWidgets::Button *ghnsButton = new KNSWidgets::Button(i18n("Get New Task Switchers..."), QStringLiteral("kwinswitcher.knsrc"), this);
    connect(ghnsButton, &KNSWidgets::Button::dialogFinished, this, [this](auto changedEntries) {
        if (!changedEntries.isEmpty()) {
            initLayoutLists();
        }
    });

    QHBoxLayout *buttonBar = new QHBoxLayout();
    QSpacerItem *buttonBarSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonBar->addItem(buttonBarSpacer);
    buttonBar->addWidget(ghnsButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    KTitleWidget *infoLabel = new KTitleWidget(tabWidget);
    infoLabel->setText(i18n("Focus policy settings limit the functionality of navigating through windows."),
                       KTitleWidget::InfoMessage);
    infoLabel->setIcon(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
    layout->addWidget(infoLabel, 0);
    layout->addWidget(tabWidget, 1);
    layout->addLayout(buttonBar);
    setLayout(layout);

    addConfig(m_data->tabBoxConfig(), m_primaryTabBoxUi);
    addConfig(m_data->tabBoxAlternativeConfig(), m_alternativeTabBoxUi);

    initLayoutLists();

    connect(this, &KWinTabBoxConfig::defaultsIndicatorsVisibleChanged, this, &KWinTabBoxConfig::updateDefaultIndicator);
    createConnections(m_primaryTabBoxUi);
    createConnections(m_alternativeTabBoxUi);

    // check focus policy - we don't offer configs for unreasonable focus policies
    KConfigGroup config(m_config, "Windows");
    QString policy = config.readEntry("FocusPolicy", "ClickToFocus");
    if ((policy == "FocusUnderMouse") || (policy == "FocusStrictlyUnderMouse")) {
        tabWidget->setEnabled(false);
        infoLabel->show();
    } else {
        infoLabel->hide();
    }

    setEnabledUi(m_primaryTabBoxUi, m_data->tabBoxConfig());
    setEnabledUi(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());
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
    QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->listPackages("KWin/WindowSwitcher");
    QStringList layoutNames, layoutPlugins, layoutPaths;

    const auto lnfPackages = availableLnFPackages();
    for (const auto &package : lnfPackages) {
        const auto &metaData = package.metadata();

        const QString switcherFile = package.filePath("windowswitcher", QStringLiteral("WindowSwitcher.qml"));
        if (switcherFile.isEmpty()) {
            // Skip lnfs that don't actually ship a switcher
            continue;
        }
        layoutNames << metaData.name();
        layoutPlugins << metaData.pluginId();
        layoutPaths << switcherFile;
    }

    for (const auto &offer : offers) {
        const QString pluginName = offer.pluginId();
        if (offer.value("X-Plasma-API") != "declarativeappletscript") {
            continue;
        }
        // we don't have a proper servicetype
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

    KWinTabBoxConfigForm *ui[2] = {m_primaryTabBoxUi, m_alternativeTabBoxUi};
    for (int i = 0; i < 2; ++i) {
        QStandardItemModel *model = new QStandardItemModel;

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
    form->setOrderMinimizedModeEnabled(!config->isOrderMinimizedModeImmutable());
    form->setShowDesktopModeEnabled(!config->isShowDesktopModeImmutable());
    form->setSwitchingModeEnabled(!config->isSwitchingModeImmutable());
    form->setLayoutNameEnabled(!config->isLayoutNameImmutable());
}

void KWinTabBoxConfig::createConnections(KWinTabBoxConfigForm *form)
{
    connect(form, &KWinTabBoxConfigForm::effectConfigButtonClicked, this, &KWinTabBoxConfig::configureEffectClicked);

    connect(form, &KWinTabBoxConfigForm::filterScreenChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::filterDesktopChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::filterActivitiesChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::filterMinimizationChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::applicationModeChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::orderMinimizedModeChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::showDesktopModeChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::switchingModeChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::layoutNameChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
    connect(form, &KWinTabBoxConfigForm::shortcutChanged, this, &KWinTabBoxConfig::updateUnmanagedState);
}

void KWinTabBoxConfig::updateUnmanagedState()
{
    bool isNeedSave = false;
    isNeedSave |= updateUnmanagedIsNeedSave(m_primaryTabBoxUi, m_data->tabBoxConfig());
    isNeedSave |= updateUnmanagedIsNeedSave(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());

    unmanagedWidgetChangeState(isNeedSave);

    bool isDefault = true;
    isDefault &= updateUnmanagedIsDefault(m_primaryTabBoxUi, m_data->tabBoxConfig());
    isDefault &= updateUnmanagedIsDefault(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());

    unmanagedWidgetDefaultState(isDefault);

    updateDefaultIndicator();
}

void KWinTabBoxConfig::updateDefaultIndicator()
{
    const bool visible = defaultsIndicatorsVisible();
    updateUiDefaultIndicator(visible, m_primaryTabBoxUi, m_data->tabBoxConfig());
    updateUiDefaultIndicator(visible, m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());
}

bool KWinTabBoxConfig::updateUnmanagedIsNeedSave(const KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    bool isNeedSave = false;
    isNeedSave |= form->filterScreen() != config->multiScreenMode();
    isNeedSave |= form->filterDesktop() != config->desktopMode();
    isNeedSave |= form->filterActivities() != config->activitiesMode();
    isNeedSave |= form->filterMinimization() != config->minimizedMode();
    isNeedSave |= form->applicationMode() != config->applicationsMode();
    isNeedSave |= form->orderMinimizedMode() != config->orderMinimizedMode();
    isNeedSave |= form->showDesktopMode() != config->showDesktopMode();
    isNeedSave |= form->switchingMode() != config->switchingMode();
    isNeedSave |= form->layoutName() != config->layoutName();
    isNeedSave |= form->isShortcutsChanged();

    return isNeedSave;
}

bool KWinTabBoxConfig::updateUnmanagedIsDefault(KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    bool isDefault = true;
    isDefault &= form->filterScreen() == config->defaultMultiScreenModeValue();
    isDefault &= form->filterDesktop() == config->defaultDesktopModeValue();
    isDefault &= form->filterActivities() == config->defaultActivitiesModeValue();
    isDefault &= form->filterMinimization() == config->defaultMinimizedModeValue();
    isDefault &= form->applicationMode() == config->defaultApplicationsModeValue();
    isDefault &= form->orderMinimizedMode() == config->defaultOrderMinimizedModeValue();
    isDefault &= form->showDesktopMode() == config->defaultShowDesktopModeValue();
    isDefault &= form->switchingMode() == config->defaultSwitchingModeValue();
    isDefault &= form->layoutName() == config->defaultLayoutNameValue();
    isDefault &= form->isShortcutsDefault();

    return isDefault;
}

void KWinTabBoxConfig::updateUiDefaultIndicator(bool visible, KWinTabBoxConfigForm *form, const TabBoxSettings *config)
{
    form->setFilterScreenDefaultIndicatorVisible(visible && form->filterScreen() != config->defaultMultiScreenModeValue());
    form->setFilterDesktopDefaultIndicatorVisible(visible && form->filterDesktop() != config->defaultDesktopModeValue());
    form->setFilterActivitiesDefaultIndicatorVisible(visible && form->filterActivities() != config->defaultActivitiesModeValue());
    form->setFilterMinimizationDefaultIndicatorVisible(visible && form->filterMinimization() != config->defaultMinimizedModeValue());
    form->setApplicationModeDefaultIndicatorVisible(visible && form->applicationMode() != config->defaultApplicationsModeValue());
    form->setOrderMinimizedDefaultIndicatorVisible(visible && form->orderMinimizedMode() != config->defaultOrderMinimizedModeValue());
    form->setShowDesktopModeDefaultIndicatorVisible(visible && form->showDesktopMode() != config->defaultShowDesktopModeValue());
    form->setSwitchingModeDefaultIndicatorVisible(visible && form->switchingMode() != config->defaultSwitchingModeValue());
    form->setLayoutNameDefaultIndicatorVisible(visible && form->layoutName() != config->defaultLayoutNameValue());
    form->setShortcutsDefaultIndicatorVisible(visible);
}

void KWinTabBoxConfig::load()
{
    KCModule::load();

    m_data->tabBoxConfig()->load();
    m_data->tabBoxAlternativeConfig()->load();

    updateUiFromConfig(m_primaryTabBoxUi, m_data->tabBoxConfig());
    updateUiFromConfig(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());

    m_data->pluginsConfig()->load();

    m_primaryTabBoxUi->loadShortcuts();
    m_alternativeTabBoxUi->loadShortcuts();

    updateUnmanagedState();
}

void KWinTabBoxConfig::save()
{
    // effects
    const bool highlightWindows = m_primaryTabBoxUi->highlightWindows() || m_alternativeTabBoxUi->highlightWindows();

    // activate effects if they are used otherwise deactivate them.
    m_data->pluginsConfig()->setHighlightwindowEnabled(highlightWindows);
    m_data->pluginsConfig()->save();

    updateConfigFromUi(m_primaryTabBoxUi, m_data->tabBoxConfig());
    updateConfigFromUi(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());

    m_primaryTabBoxUi->saveShortcuts();
    m_alternativeTabBoxUi->saveShortcuts();

    m_data->tabBoxConfig()->save();
    m_data->tabBoxAlternativeConfig()->save();

    KCModule::save();
    updateUnmanagedState();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KWinTabBoxConfig::defaults()
{
    updateUiFromDefaultConfig(m_primaryTabBoxUi, m_data->tabBoxConfig());
    updateUiFromDefaultConfig(m_alternativeTabBoxUi, m_data->tabBoxAlternativeConfig());

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
    form->setOrderMinimizedMode(static_cast<TabBoxConfig::OrderMinimizedMode>(config->orderMinimizedMode()));
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
    config->setOrderMinimizedMode(form->orderMinimizedMode());
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
    form->setOrderMinimizedMode(static_cast<TabBoxConfig::OrderMinimizedMode>(config->defaultOrderMinimizedModeValue()));
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
        new LayoutPreview(form->effectComboCurrentData(KWinTabBoxConfigForm::LayoutPath).toString(),
                          form->showDesktopMode(),
                          this);
    }
}

} // namespace

#include "main.moc"
