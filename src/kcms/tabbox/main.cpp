/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>
    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main.h"

#include "config-kwin.h"

#include <kwin_effects_interface.h>

// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

// KDE
#include <KLocalizedString>
#include <KNSWidgets/Button>
#include <KPluginFactory>
#include <KSeparator>
#include <KTitleWidget>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>

// own
#include "kwintabboxconfigform.h"
#include "kwintabboxdata.h"
#include "kwintabboxsettings.h"
#include "shortcutsettings.h"

#include <QTabBar>

K_PLUGIN_FACTORY_WITH_JSON(KWinTabBoxConfigFactory, "kcm_kwintabbox.json", registerPlugin<KWin::KWinTabBoxConfig>(); registerPlugin<KWin::TabBox::KWinTabboxData>();)

namespace KWin
{

using namespace TabBox;

KWinTabBoxConfig::KWinTabBoxConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_data(new KWinTabboxData(this))
{
    QTabWidget *tabWidget = new QTabWidget(widget());
    tabWidget->setDocumentMode(true);
    tabWidget->tabBar()->setExpanding(true);

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

    KNSWidgets::Button *ghnsButton = new KNSWidgets::Button(i18n("Get New Task Switcher Styles…"), QStringLiteral("kwinswitcher.knsrc"), widget());
    connect(ghnsButton, &KNSWidgets::Button::dialogFinished, this, [this](auto changedEntries) {
        if (!changedEntries.isEmpty()) {
            initLayoutLists();
        }
    });

    QHBoxLayout *buttonBar = new QHBoxLayout();
    QStyle *style = widget()->style();

    buttonBar->setContentsMargins(style->pixelMetric(QStyle::PM_LayoutLeftMargin), 0, style->pixelMetric(QStyle::PM_LayoutRightMargin), style->pixelMetric(QStyle::PM_LayoutBottomMargin));
    QSpacerItem *buttonBarSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonBar->addItem(buttonBarSpacer);
    buttonBar->addWidget(ghnsButton);

    QVBoxLayout *layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    KTitleWidget *infoLabel = new KTitleWidget(tabWidget);
    infoLabel->setText(i18n("Focus policy settings limit the functionality of navigating through windows."),
                       KTitleWidget::InfoMessage);
    infoLabel->setIcon(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
    layout->addWidget(infoLabel, 0);
    layout->addWidget(tabWidget, 1);
    KSeparator *separator = new KSeparator();
    layout->addWidget(separator);
    layout->addLayout(buttonBar);
    widget()->setLayout(layout);

    // Hide the separator if KNS is disabled.
    separator->setVisible(!ghnsButton->isHidden());

    addConfig(m_data->tabBoxConfig(), m_primaryTabBoxUi);
    addConfig(m_data->tabBoxAlternativeConfig(), m_alternativeTabBoxUi);

    initLayoutLists();

    createConnections(m_primaryTabBoxUi);
    createConnections(m_alternativeTabBoxUi);

    // check focus policy - we don't offer configs for unreasonable focus policies
    KConfigGroup config(m_config, QStringLiteral("Windows"));
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
            KConfigGroup cg = KConfigGroup(conf, QStringLiteral("kwinrc"));
            cg = KConfigGroup(&cg, QStringLiteral("WindowSwitcher"));
            if (!cg.readEntry("LayoutName", QString()).isEmpty()) {
                packages << pkg;
            }
        }
    }

    return packages;
}

void KWinTabBoxConfig::initLayoutLists()
{
    auto model = std::make_unique<QStandardItemModel>();

    auto addToModel = [model = model.get()](const QString &name, const QString &pluginId, const QString &path) {
        QStandardItem *item = new QStandardItem(name);
        item->setData(pluginId, Qt::UserRole);
        item->setData(path, KWinTabBoxConfigForm::LayoutPath);
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

    const QStringList packageRoots{
        QStringLiteral("kwin-wayland/tabbox"),
        QStringLiteral("kwin/tabbox"),
    };
    for (const QString &packageRoot : packageRoots) {
        const QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/WindowSwitcher"), packageRoot);
        for (const auto &offer : offers) {
            const QString pluginName = offer.pluginId();
            const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                              packageRoot + QLatin1Char('/') + pluginName + QLatin1String("/contents/ui/main.qml"));
            if (scriptFile.isEmpty()) {
                qWarning() << "scriptfile is null" << pluginName;
                continue;
            }

            addToModel(offer.name(), pluginName, scriptFile);
        }
    }

    model->sort(0);

    m_primaryTabBoxUi->setEffectComboModel(model.get());
    m_alternativeTabBoxUi->setEffectComboModel(model.get());

    m_switcherModel = std::move(model);
}

void KWinTabBoxConfig::createConnections(KWinTabBoxConfigForm *form)
{
    connect(form, &KWinTabBoxConfigForm::effectPreviewClicked, this, &KWinTabBoxConfig::showPreview);
    connect(form, &KWinTabBoxConfigForm::configChanged, this, &KWinTabBoxConfig::updateUnmanagedState);

    connect(this, &KWinTabBoxConfig::defaultsIndicatorsVisibleChanged, form, [form, this]() {
        form->setDefaultIndicatorVisible(defaultsIndicatorsVisible());
    });
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

    m_primaryTabBoxUi->updateUiFromConfig();
    m_alternativeTabBoxUi->updateUiFromConfig();

    updateUnmanagedState();
}

void KWinTabBoxConfig::save()
{
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

void KWinTabBoxConfig::showPreview()
{
    auto form = qobject_cast<KWinTabBoxConfigForm *>(sender());
    Q_ASSERT(form);

    // The process will close when losing focus, but check in case of multiple calls
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
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
    args << form->effectComboCurrentData(KWinTabBoxConfigForm::LayoutPath).toString();
    if (form->config()->showDesktopMode()) {
        args << QStringLiteral("--show-desktop");
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_FIXED_POSITIONS"),
               QStringLiteral("1"));

    m_previewProcess = std::make_unique<QProcess>();
    m_previewProcess->setArguments(args);
    m_previewProcess->setProgram(previewHelper);
    m_previewProcess->setProcessEnvironment(env);
    m_previewProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_previewProcess->start();
}

} // namespace

#include "main.moc"

#include "moc_main.cpp"
