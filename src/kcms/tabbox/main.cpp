/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>
    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

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
#include "shortcutsettings.h"

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
    m_primaryTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Main,
                                                 m_data->tabBoxConfig(),
                                                 m_data->shortcutConfig(),
                                                 tabWidget);
    m_alternativeTabBoxUi = new KWinTabBoxConfigForm(KWinTabBoxConfigForm::TabboxType::Alternative,
                                                     m_data->tabBoxAlternativeConfig(),
                                                     m_data->shortcutConfig(),
                                                     tabWidget);
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
    QStandardItemModel *model = new QStandardItemModel;

    auto addToModel = [model](const QString &name, const QString &pluginId, const QString &path) {
        QStandardItem *item = new QStandardItem(name);
        item->setData(pluginId, Qt::UserRole);
        item->setData(path, KWinTabBoxConfigForm::LayoutPath);
        item->setData(true, KWinTabBoxConfigForm::AddonEffect);
        model->appendRow(item);
    };

    const auto lnfPackages = availableLnFPackages();
    for (const auto &package : lnfPackages) {
        const auto &metaData = package.metadata();
        const QString switcherFile = package.filePath("windowswitcher", QStringLiteral("WindowSwitcher.qml"));
        if (switcherFile.isEmpty()) {
            // Skip lnfs that don't actually ship a switcher
            continue;
        }

        addToModel(metaData.name(), metaData.pluginId(), switcherFile);
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

        addToModel(offer.name(), pluginName, scriptFile);
    }

    model->sort(0);

    m_primaryTabBoxUi->setEffectComboModel(model);
    m_alternativeTabBoxUi->setEffectComboModel(model);
}

void KWinTabBoxConfig::createConnections(KWinTabBoxConfigForm *form)
{
    connect(form, &KWinTabBoxConfigForm::effectConfigButtonClicked, this, &KWinTabBoxConfig::configureEffectClicked);
    connect(form, &KWinTabBoxConfigForm::configChanged, this, &KWinTabBoxConfig::updateUnmanagedState);

    connect(this, &KWinTabBoxConfig::defaultsIndicatorsVisibleChanged, form, &KWinTabBoxConfigForm::setDefaultIndicatorVisible);
}

void KWinTabBoxConfig::updateUnmanagedState()
{
    const bool isNeedSave = m_data->tabBoxConfig()->isSaveNeeded()
        || m_data->tabBoxAlternativeConfig()->isSaveNeeded()
        || m_data->shortcutConfig()->isSaveNeeded();

    unmanagedWidgetChangeState(isNeedSave);

    const bool isDefault = m_data->tabBoxConfig()->isDefaults()
        && m_data->tabBoxAlternativeConfig()->isDefaults()
        && m_data->shortcutConfig()->isDefaults();

    unmanagedWidgetDefaultState(isDefault);
}

void KWinTabBoxConfig::load()
{
    KCModule::load();

    m_data->tabBoxConfig()->load();
    m_data->tabBoxAlternativeConfig()->load();
    m_data->shortcutConfig()->load();

    m_data->pluginsConfig()->load();

    m_primaryTabBoxUi->updateUiFromConfig();
    m_alternativeTabBoxUi->updateUiFromConfig();

    updateUnmanagedState();
}

void KWinTabBoxConfig::save()
{
    // effects
    const bool highlightWindows = m_primaryTabBoxUi->highlightWindows() || m_alternativeTabBoxUi->highlightWindows();

    // activate effects if they are used otherwise deactivate them.
    m_data->pluginsConfig()->setHighlightwindowEnabled(highlightWindows);
    m_data->pluginsConfig()->save();

    m_data->tabBoxConfig()->save();
    m_data->tabBoxAlternativeConfig()->save();
    m_data->shortcutConfig()->save();

    KCModule::save();
    updateUnmanagedState();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KWinTabBoxConfig::defaults()
{
    m_data->tabBoxConfig()->setDefaults();
    m_data->tabBoxAlternativeConfig()->setDefaults();
    m_data->shortcutConfig()->setDefaults();

    m_primaryTabBoxUi->updateUiFromConfig();
    m_alternativeTabBoxUi->updateUiFromConfig();

    KCModule::defaults();
    updateUnmanagedState();
}
void KWinTabBoxConfig::configureEffectClicked()
{
    auto form = qobject_cast<KWinTabBoxConfigForm *>(sender());
    Q_ASSERT(form);

    if (form->effectComboCurrentData(KWinTabBoxConfigForm::AddonEffect).toBool()) {
        // Show the preview for addon effect
        new LayoutPreview(form->effectComboCurrentData(KWinTabBoxConfigForm::LayoutPath).toString(),
                          form->config()->desktopMode(),
                          this);
    }
}

} // namespace

#include "main.moc"
