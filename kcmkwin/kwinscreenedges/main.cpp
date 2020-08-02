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
#include "kwinscreenedgesettings.h"
#include "kwinscreenedgescriptsettings.h"

K_PLUGIN_FACTORY(KWinScreenEdgesConfigFactory, registerPlugin<KWin::KWinScreenEdgesConfig>();)

namespace KWin
{

KWinScreenEdgesConfig::KWinScreenEdgesConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_form(new KWinScreenEdgesConfigForm(this))
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_settings(new KWinScreenEdgeSettings(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_form);

    addConfig(m_settings, m_form);

    monitorInit();

    connect(m_form, &KWinScreenEdgesConfigForm::saveNeededChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetChangeState);
    connect(m_form, &KWinScreenEdgesConfigForm::defaultChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetDefaultState);
}

KWinScreenEdgesConfig::~KWinScreenEdgesConfig()
{
}

void KWinScreenEdgesConfig::load()
{
    KCModule::load();
    m_settings->load();
    for (KWinScreenEdgeScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->load();
    }

    monitorLoadSettings();
    monitorLoadDefaultSettings();
    m_form->setElectricBorderCornerRatio(m_settings->electricBorderCornerRatio());
    m_form->setDefaultElectricBorderCornerRatio(m_settings->defaultElectricBorderCornerRatioValue());
    m_form->reload();
}

void KWinScreenEdgesConfig::save()
{
    monitorSaveSettings();
    m_settings->setElectricBorderCornerRatio(m_form->electricBorderCornerRatio());
    m_settings->save();
    for (KWinScreenEdgeScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->save();
    }

    // Reload saved settings to ScreenEdge UI
    monitorLoadSettings();
    m_form->setElectricBorderCornerRatio(m_settings->electricBorderCornerRatio());
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
    m_form->monitorChangeEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->top()));
    m_form->monitorChangeEdge(ElectricTopRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->topRight()));
    m_form->monitorChangeEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->right()));
    m_form->monitorChangeEdge(ElectricBottomRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->bottomRight()));
    m_form->monitorChangeEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->bottom()));
    m_form->monitorChangeEdge(ElectricBottomLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->bottomLeft()));
    m_form->monitorChangeEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->left()));
    m_form->monitorChangeEdge(ElectricTopLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->topLeft()));

    // Load effect-specific actions:

    // PresentWindows BorderActivateAll
    m_form->monitorChangeEdge(m_settings->borderActivateAll(), PresentWindowsAll);

    // PresentWindows BorderActivate
    m_form->monitorChangeEdge(m_settings->borderActivatePresentWindows(), PresentWindowsCurrent);

    // PresentWindows BorderActivateClass
    m_form->monitorChangeEdge(m_settings->borderActivateClass(), PresentWindowsClass);

    // Desktop Grid
    m_form->monitorChangeEdge(m_settings->borderActivateDesktopGrid(), DesktopGrid);

    // Desktop Cube
    m_form->monitorChangeEdge(m_settings->borderActivateCube(), Cube);
    m_form->monitorChangeEdge(m_settings->borderActivateCylinder(), Cylinder);
    m_form->monitorChangeEdge(m_settings->borderActivateSphere(), Sphere);

    // TabBox
    m_form->monitorChangeEdge(m_settings->borderActivateTabBox(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeEdge(m_settings->borderAlternativeActivate(), TabBoxAlternative);

    // Scripts
    for (int i = 0; i < m_scripts.size(); i++) {
        int index = EffectCount + i;
        m_form->monitorChangeEdge(m_scriptSettings[m_scripts[i]]->borderActivate(), index);
    }
}

void KWinScreenEdgesConfig::monitorLoadDefaultSettings()
{
    // Load ElectricBorderActions
    m_form->monitorChangeDefaultEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultTopValue()));
    m_form->monitorChangeDefaultEdge(ElectricTopRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultTopRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottomRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultBottomRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultBottomValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottomLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultBottomLeftValue()));
    m_form->monitorChangeDefaultEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultLeftValue()));
    m_form->monitorChangeDefaultEdge(ElectricTopLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultTopLeftValue()));

    // Load effect-specific actions:

    // PresentWindows BorderActivateAll
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateAllValue(), PresentWindowsAll);

    // PresentWindows BorderActivate
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivatePresentWindowsValue(), PresentWindowsCurrent);

    // PresentWindows BorderActivateClass
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateClassValue(), PresentWindowsClass);

    // Desktop Grid
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateDesktopGridValue(), DesktopGrid);

    // Desktop Cube
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateCubeValue(), Cube);
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateCylinderValue(), Cylinder);
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateSphereValue(), Sphere);

    // TabBox
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderActivateTabBoxValue(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeDefaultEdge(m_settings->defaultBorderAlternativeActivateValue(), TabBoxAlternative);
}

void KWinScreenEdgesConfig::monitorSaveSettings()
{
    // Save ElectricBorderActions
    m_settings->setTop(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTop)));
    m_settings->setTopRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTopRight)));
    m_settings->setRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricRight)));
    m_settings->setBottomRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottomRight)));
    m_settings->setBottom(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottom)));
    m_settings->setBottomLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottomLeft)));
    m_settings->setLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricLeft)));
    m_settings->setTopLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTopLeft)));

    // Save effect-specific actions:

    // Present Windows
    m_settings->setBorderActivateAll(m_form->monitorCheckEffectHasEdge(PresentWindowsAll));
    m_settings->setBorderActivatePresentWindows(m_form->monitorCheckEffectHasEdge(PresentWindowsCurrent));
    m_settings->setBorderActivateClass(m_form->monitorCheckEffectHasEdge(PresentWindowsClass));

    // Desktop Grid
    m_settings->setBorderActivateDesktopGrid(m_form->monitorCheckEffectHasEdge(DesktopGrid));

    // Desktop Cube
    m_settings->setBorderActivateCube(m_form->monitorCheckEffectHasEdge(Cube));
    m_settings->setBorderActivateCylinder(m_form->monitorCheckEffectHasEdge(Cylinder));
    m_settings->setBorderActivateSphere(m_form->monitorCheckEffectHasEdge(Sphere));

    // TabBox
    m_settings->setBorderActivateTabBox(m_form->monitorCheckEffectHasEdge(TabBox));
    m_settings->setBorderAlternativeActivate(m_form->monitorCheckEffectHasEdge(TabBoxAlternative));

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
    m_form->monitorEnableEdge(ElectricTop, !m_settings->isTopImmutable());
    m_form->monitorEnableEdge(ElectricTopRight, !m_settings->isTopRightImmutable());
    m_form->monitorEnableEdge(ElectricRight, !m_settings->isRightImmutable());
    m_form->monitorEnableEdge(ElectricBottomRight, !m_settings->isBottomRightImmutable());
    m_form->monitorEnableEdge(ElectricBottom, !m_settings->isBottomImmutable());
    m_form->monitorEnableEdge(ElectricBottomLeft, !m_settings->isBottomLeftImmutable());
    m_form->monitorEnableEdge(ElectricLeft, !m_settings->isLeftImmutable());
    m_form->monitorEnableEdge(ElectricTopLeft, !m_settings->isTopLeftImmutable());

    // Disable ElectricBorderCornerRatio if entry is immutable
    m_form->setElectricBorderCornerRatioEnabled(!m_settings->isElectricBorderCornerRatioImmutable());
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
