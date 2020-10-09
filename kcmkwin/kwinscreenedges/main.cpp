/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "main.h"
#include <effect_builtins.h>
#include <kwin_effects_interface.h>

#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <QtDBus>
#include <QVBoxLayout>

#include "kwinscreenedgeconfigform.h"
#include "kwinscreenedgedata.h"
#include "kwinscreenedgesettings.h"
#include "kwinscreenedgescriptsettings.h"

K_PLUGIN_FACTORY(KWinScreenEdgesConfigFactory, registerPlugin<KWin::KWinScreenEdgesConfig>(); registerPlugin<KWin::KWinScreenEdgeData>();)

namespace KWin
{

KWinScreenEdgesConfig::KWinScreenEdgesConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_form(new KWinScreenEdgesConfigForm(this))
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_data(new KWinScreenEdgeData(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_form);

    addConfig(m_data->settings(), m_form);

    monitorInit();

    connect(this, &KWinScreenEdgesConfig::defaultsIndicatorsVisibleChanged, m_form, &KWinScreenEdgesConfigForm::setDefaultsIndicatorsVisible);
    connect(m_form, &KWinScreenEdgesConfigForm::saveNeededChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetChangeState);
    connect(m_form, &KWinScreenEdgesConfigForm::defaultChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetDefaultState);
}

KWinScreenEdgesConfig::~KWinScreenEdgesConfig()
{
}

void KWinScreenEdgesConfig::load()
{
    KCModule::load();
    m_data->settings()->load();
    for (KWinScreenEdgeScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->load();
    }

    monitorLoadSettings();
    monitorLoadDefaultSettings();
    m_form->setElectricBorderCornerRatio(m_data->settings()->electricBorderCornerRatio());
    m_form->setDefaultElectricBorderCornerRatio(m_data->settings()->defaultElectricBorderCornerRatioValue());
    m_form->reload();
}

void KWinScreenEdgesConfig::save()
{
    monitorSaveSettings();
    m_data->settings()->setElectricBorderCornerRatio(m_form->electricBorderCornerRatio());
    m_data->settings()->save();
    for (KWinScreenEdgeScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->save();
    }

    // Reload saved settings to ScreenEdge UI
    monitorLoadSettings();
    m_form->setElectricBorderCornerRatio(m_data->settings()->electricBorderCornerRatio());
    m_form->reload();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
    // and reconfigure the effects
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Effects"),
                                             QDBusConnection::sessionBus());
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::PresentWindows));
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::DesktopGrid));
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::Cube));

    KCModule::save();
}

void KWinScreenEdgesConfig::defaults()
{
    m_form->setDefaults();

    KCModule::defaults();
}

void KWinScreenEdgesConfig::showEvent(QShowEvent *e)
{
    KCModule::showEvent(e);

    monitorShowEvent();
}

// Copied from kcmkwin/kwincompositing/main.cpp
bool KWinScreenEdgesConfig::effectEnabled(const BuiltInEffect &effect, const KConfigGroup &cfg) const
{
    return cfg.readEntry(BuiltInEffects::nameForEffect(effect) + "Enabled", BuiltInEffects::enabledByDefault(effect));
}

//-----------------------------------------------------------------------------
// Monitor

void KWinScreenEdgesConfig::monitorInit()
{
    m_form->monitorAddItem(i18n("No Action"));
    m_form->monitorAddItem(i18n("Show Desktop"));
    m_form->monitorAddItem(i18n("Lock Screen"));
    m_form->monitorAddItem(i18n("Show KRunner"));
    m_form->monitorAddItem(i18n("Activity Manager"));
    m_form->monitorAddItem(i18n("Application Launcher"));

    // Add the effects
    const QString presentWindowsName = BuiltInEffects::effectData(BuiltInEffect::PresentWindows).displayName;
    m_form->monitorAddItem(i18n("%1 - All Desktops", presentWindowsName));
    m_form->monitorAddItem(i18n("%1 - Current Desktop", presentWindowsName));
    m_form->monitorAddItem(i18n("%1 - Current Application", presentWindowsName));
    m_form->monitorAddItem(BuiltInEffects::effectData(BuiltInEffect::DesktopGrid).displayName);
    const QString cubeName = BuiltInEffects::effectData(BuiltInEffect::Cube).displayName;
    m_form->monitorAddItem(i18n("%1 - Cube", cubeName));
    m_form->monitorAddItem(i18n("%1 - Cylinder", cubeName));
    m_form->monitorAddItem(i18n("%1 - Sphere", cubeName));

    m_form->monitorAddItem(i18n("Toggle window switching"));
    m_form->monitorAddItem(i18n("Toggle alternative window switching"));

    const QString scriptFolder = QStringLiteral("kwin/scripts/");
    const auto scripts = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/Script"), scriptFolder);

    KConfigGroup config(m_config, "Plugins");
    for (const KPluginMetaData &script: scripts) {
        if (script.value(QStringLiteral("X-KWin-Border-Activate")) != QLatin1String("true")) {
            continue;
        }

        if (!config.readEntry(script.pluginId() + QStringLiteral("Enabled"), script.isEnabledByDefault())) {
            continue;
        }
        m_scripts << script.pluginId();
        m_form->monitorAddItem(script.name());
        m_scriptSettings[script.pluginId()] = new KWinScreenEdgeScriptSettings(script.pluginId(), this);
    }

    monitorShowEvent();
}

void KWinScreenEdgesConfig::monitorLoadSettings()
{
    // Load ElectricBorderActions
    m_form->monitorChangeEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->top()));
    m_form->monitorChangeEdge(ElectricTopRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->topRight()));
    m_form->monitorChangeEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->right()));
    m_form->monitorChangeEdge(ElectricBottomRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->bottomRight()));
    m_form->monitorChangeEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->bottom()));
    m_form->monitorChangeEdge(ElectricBottomLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->bottomLeft()));
    m_form->monitorChangeEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->left()));
    m_form->monitorChangeEdge(ElectricTopLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->topLeft()));

    // Load effect-specific actions:

    // PresentWindows BorderActivateAll
    m_form->monitorChangeEdge(m_data->settings()->borderActivateAll(), PresentWindowsAll);

    // PresentWindows BorderActivate
    m_form->monitorChangeEdge(m_data->settings()->borderActivatePresentWindows(), PresentWindowsCurrent);

    // PresentWindows BorderActivateClass
    m_form->monitorChangeEdge(m_data->settings()->borderActivateClass(), PresentWindowsClass);

    // Desktop Grid
    m_form->monitorChangeEdge(m_data->settings()->borderActivateDesktopGrid(), DesktopGrid);

    // Desktop Cube
    m_form->monitorChangeEdge(m_data->settings()->borderActivateCube(), Cube);
    m_form->monitorChangeEdge(m_data->settings()->borderActivateCylinder(), Cylinder);
    m_form->monitorChangeEdge(m_data->settings()->borderActivateSphere(), Sphere);

    // TabBox
    m_form->monitorChangeEdge(m_data->settings()->borderActivateTabBox(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeEdge(m_data->settings()->borderAlternativeActivate(), TabBoxAlternative);

    // Scripts
    for (int i = 0; i < m_scripts.size(); i++) {
        int index = EffectCount + i;
        m_form->monitorChangeEdge(m_scriptSettings[m_scripts[i]]->borderActivate(), index);
    }
}

void KWinScreenEdgesConfig::monitorLoadDefaultSettings()
{
    // Load ElectricBorderActions
    m_form->monitorChangeDefaultEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultTopValue()));
    m_form->monitorChangeDefaultEdge(ElectricTopRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultTopRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottomRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultBottomRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultBottomValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottomLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultBottomLeftValue()));
    m_form->monitorChangeDefaultEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultLeftValue()));
    m_form->monitorChangeDefaultEdge(ElectricTopLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_data->settings()->defaultTopLeftValue()));

    // Load effect-specific actions:

    // PresentWindows BorderActivateAll
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateAllValue(), PresentWindowsAll);

    // PresentWindows BorderActivate
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivatePresentWindowsValue(), PresentWindowsCurrent);

    // PresentWindows BorderActivateClass
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateClassValue(), PresentWindowsClass);

    // Desktop Grid
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateDesktopGridValue(), DesktopGrid);

    // Desktop Cube
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateCubeValue(), Cube);
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateCylinderValue(), Cylinder);
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateSphereValue(), Sphere);

    // TabBox
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderActivateTabBoxValue(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeDefaultEdge(m_data->settings()->defaultBorderAlternativeActivateValue(), TabBoxAlternative);
}

void KWinScreenEdgesConfig::monitorSaveSettings()
{
    // Save ElectricBorderActions
    m_data->settings()->setTop(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTop)));
    m_data->settings()->setTopRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTopRight)));
    m_data->settings()->setRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricRight)));
    m_data->settings()->setBottomRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottomRight)));
    m_data->settings()->setBottom(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottom)));
    m_data->settings()->setBottomLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottomLeft)));
    m_data->settings()->setLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricLeft)));
    m_data->settings()->setTopLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTopLeft)));

    // Save effect-specific actions:

    // Present Windows
    m_data->settings()->setBorderActivateAll(m_form->monitorCheckEffectHasEdge(PresentWindowsAll));
    m_data->settings()->setBorderActivatePresentWindows(m_form->monitorCheckEffectHasEdge(PresentWindowsCurrent));
    m_data->settings()->setBorderActivateClass(m_form->monitorCheckEffectHasEdge(PresentWindowsClass));

    // Desktop Grid
    m_data->settings()->setBorderActivateDesktopGrid(m_form->monitorCheckEffectHasEdge(DesktopGrid));

    // Desktop Cube
    m_data->settings()->setBorderActivateCube(m_form->monitorCheckEffectHasEdge(Cube));
    m_data->settings()->setBorderActivateCylinder(m_form->monitorCheckEffectHasEdge(Cylinder));
    m_data->settings()->setBorderActivateSphere(m_form->monitorCheckEffectHasEdge(Sphere));

    // TabBox
    m_data->settings()->setBorderActivateTabBox(m_form->monitorCheckEffectHasEdge(TabBox));
    m_data->settings()->setBorderAlternativeActivate(m_form->monitorCheckEffectHasEdge(TabBoxAlternative));

    // Scripts
    for (int i = 0; i < m_scripts.size(); i++) {
        int index = EffectCount + i;
        m_scriptSettings[m_scripts[i]]->setBorderActivate(m_form->monitorCheckEffectHasEdge(index));
    }
}

void KWinScreenEdgesConfig::monitorShowEvent()
{
    // Check if they are enabled
    KConfigGroup config(m_config, "Plugins");

    // Present Windows
    bool enabled = effectEnabled(BuiltInEffect::PresentWindows, config);
    m_form->monitorItemSetEnabled(PresentWindowsCurrent, enabled);
    m_form->monitorItemSetEnabled(PresentWindowsAll, enabled);

    // Desktop Grid
    enabled = effectEnabled(BuiltInEffect::DesktopGrid, config);
    m_form->monitorItemSetEnabled(DesktopGrid, enabled);

    // Desktop Cube
    enabled = effectEnabled(BuiltInEffect::Cube, config);
    m_form->monitorItemSetEnabled(Cube, enabled);
    m_form->monitorItemSetEnabled(Cylinder, enabled);
    m_form->monitorItemSetEnabled(Sphere, enabled);
    // tabbox, depends on reasonable focus policy.
    KConfigGroup config2(m_config, "Windows");
    QString focusPolicy = config2.readEntry("FocusPolicy", QString());
    bool reasonable = focusPolicy != "FocusStrictlyUnderMouse" && focusPolicy != "FocusUnderMouse";
    m_form->monitorItemSetEnabled(TabBox, reasonable);
    m_form->monitorItemSetEnabled(TabBoxAlternative, reasonable);

    // Disable Edge if ElectricBorders group entries are immutable
    m_form->monitorEnableEdge(ElectricTop, !m_data->settings()->isTopImmutable());
    m_form->monitorEnableEdge(ElectricTopRight, !m_data->settings()->isTopRightImmutable());
    m_form->monitorEnableEdge(ElectricRight, !m_data->settings()->isRightImmutable());
    m_form->monitorEnableEdge(ElectricBottomRight, !m_data->settings()->isBottomRightImmutable());
    m_form->monitorEnableEdge(ElectricBottom, !m_data->settings()->isBottomImmutable());
    m_form->monitorEnableEdge(ElectricBottomLeft, !m_data->settings()->isBottomLeftImmutable());
    m_form->monitorEnableEdge(ElectricLeft, !m_data->settings()->isLeftImmutable());
    m_form->monitorEnableEdge(ElectricTopLeft, !m_data->settings()->isTopLeftImmutable());

    // Disable ElectricBorderCornerRatio if entry is immutable
    m_form->setElectricBorderCornerRatioEnabled(!m_data->settings()->isElectricBorderCornerRatioImmutable());
}

ElectricBorderAction KWinScreenEdgesConfig::electricBorderActionFromString(const QString &string)
{
    QString lowerName = string.toLower();
    if (lowerName == QStringLiteral("showdesktop")) {
        return ElectricActionShowDesktop;
    }
    if (lowerName == QStringLiteral("lockscreen")) {
        return ElectricActionLockScreen;
    }
    if (lowerName == QStringLiteral("krunner")) {
        return ElectricActionKRunner;
    }
    if (lowerName == QStringLiteral("activitymanager")) {
        return ElectricActionActivityManager;
    }
    if (lowerName == QStringLiteral("applicationlauncher")) {
        return ElectricActionApplicationLauncher;
    }
    return ElectricActionNone;
}

QString KWinScreenEdgesConfig::electricBorderActionToString(int action)
{
    switch (action) {
    case 1:
        return QStringLiteral("ShowDesktop");
    case 2:
        return QStringLiteral("LockScreen");
    case 3:
        return QStringLiteral("KRunner");
    case 4:
        return QStringLiteral("ActivityManager");
    case 5:
        return QStringLiteral("ApplicationLauncher");
    default:
        return QStringLiteral("None");
    }
}

} // namespace

#include "main.moc"
