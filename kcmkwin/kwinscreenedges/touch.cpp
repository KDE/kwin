/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touch.h"
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

#include "kwintouchscreenedgeconfigform.h"
#include "kwintouchscreensettings.h"
#include "kwintouchscreenscriptsettings.h"

K_PLUGIN_FACTORY(KWinScreenEdgesConfigFactory, registerPlugin<KWin::KWinScreenEdgesConfig>();)

namespace KWin
{

KWinScreenEdgesConfig::KWinScreenEdgesConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_form(new KWinTouchScreenEdgeConfigForm(this))
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_settings(new KWinTouchScreenSettings(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_form);

    monitorInit();

    connect(m_form, &KWinTouchScreenEdgeConfigForm::saveNeededChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetChangeState);
    connect(m_form, &KWinTouchScreenEdgeConfigForm::defaultChanged, this, &KWinScreenEdgesConfig::unmanagedWidgetDefaultState);
}

KWinScreenEdgesConfig::~KWinScreenEdgesConfig()
{
}

void KWinScreenEdgesConfig::load()
{
    KCModule::load();
    m_settings->load();
    for (KWinTouchScreenScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->load();
    }

    monitorLoadSettings();
    monitorLoadDefaultSettings();
    m_form->reload();
}

void KWinScreenEdgesConfig::save()
{
    monitorSaveSettings();
    m_settings->save();
    for (KWinTouchScreenScriptSettings *setting : qAsConst(m_scriptSettings)) {
        setting->save();
    }

    // Reload saved settings to ScreenEdge UI
    monitorLoadSettings();
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

void KWinScreenEdgesConfig::showEvent(QShowEvent* e)
{
    KCModule::showEvent(e);

    monitorShowEvent();
}

// Copied from kcmkwin/kwincompositing/main.cpp
bool KWinScreenEdgesConfig::effectEnabled(const BuiltInEffect& effect, const KConfigGroup& cfg) const
{
    return cfg.readEntry(BuiltInEffects::nameForEffect(effect) + "Enabled", BuiltInEffects::enabledByDefault(effect));
}

//-----------------------------------------------------------------------------
// Monitor

void KWinScreenEdgesConfig::monitorInit()
{
    m_form->monitorHideEdge(ElectricTopLeft, true);
    m_form->monitorHideEdge(ElectricTopRight, true);
    m_form->monitorHideEdge(ElectricBottomRight, true);
    m_form->monitorHideEdge(ElectricBottomLeft, true);

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
        m_scriptSettings[script.pluginId()] = new KWinTouchScreenScriptSettings(script.pluginId(), this);
    }

    monitorShowEvent();
}

void KWinScreenEdgesConfig::monitorLoadSettings()
{
    // Load ElectricBorderActions
    m_form->monitorChangeEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->top()));
    m_form->monitorChangeEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->right()));
    m_form->monitorChangeEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->bottom()));
    m_form->monitorChangeEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->left()));

    // Load effect-specific actions:

    // Present Windows BorderActivateAll
    m_form->monitorChangeEdge(m_settings->touchBorderActivateAll(), PresentWindowsAll);
    // PresentWindows BorderActivate
    m_form->monitorChangeEdge(m_settings->touchBorderActivatePresentWindows(), PresentWindowsCurrent);
    // PresentWindows BorderActivateClass
    m_form->monitorChangeEdge(m_settings->touchBorderActivateClass(), PresentWindowsClass);

    // Desktop Grid BorderActivate
    m_form->monitorChangeEdge(m_settings->touchBorderActivateDesktopGrid(), DesktopGrid);

    // Desktop Cube BorderActivate
    m_form->monitorChangeEdge(m_settings->touchBorderActivateCube(), Cube);
    // Desktop Cube BorderActivateCylinder
    m_form->monitorChangeEdge(m_settings->touchBorderActivateCylinder(), Cylinder);
    // Desktop Cube BorderActivateSphere
    m_form->monitorChangeEdge(m_settings->touchBorderActivateSphere(), Sphere);

    // TabBox BorderActivate
    m_form->monitorChangeEdge(m_settings->touchBorderActivateTabBox(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeEdge(m_settings->touchBorderAlternativeActivate(), TabBoxAlternative);

    // Scripts
    for (int i=0; i < m_scripts.size(); i++) {
        int index = EffectCount + i;
        m_form->monitorChangeEdge(m_scriptSettings[m_scripts[i]]->touchBorderActivate(), index);
    }
}

void KWinScreenEdgesConfig::monitorLoadDefaultSettings()
{
    m_form->monitorChangeDefaultEdge(ElectricTop, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultTopValue()));
    m_form->monitorChangeDefaultEdge(ElectricRight, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultRightValue()));
    m_form->monitorChangeDefaultEdge(ElectricBottom, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultBottomValue()));
    m_form->monitorChangeDefaultEdge(ElectricLeft, KWinScreenEdgesConfig::electricBorderActionFromString(m_settings->defaultLeftValue()));

    // Present Windows BorderActivateAll
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateAllValue(), PresentWindowsAll);
    // PresentWindows BorderActivate
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivatePresentWindowsValue(), PresentWindowsCurrent);
    // PresentWindows BorderActivateClass
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateClassValue(), PresentWindowsClass);

    // Desktop Grid BorderActivate
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateDesktopGridValue(), DesktopGrid);

    // Desktop Cube BorderActivate
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateCubeValue(), Cube);
    // Desktop Cube BorderActivateCylinder
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateCylinderValue(), Cylinder);
    // Desktop Cube BorderActivateSphere
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateSphereValue(), Sphere);

    // TabBox BorderActivate
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderActivateTabBoxValue(), TabBox);
    // Alternative TabBox
    m_form->monitorChangeDefaultEdge(m_settings->defaultTouchBorderAlternativeActivateValue(), TabBoxAlternative);
}

void KWinScreenEdgesConfig::monitorSaveSettings()
{
    // Save ElectricBorderActions
    m_settings->setTop(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricTop)));
    m_settings->setRight(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricRight)));
    m_settings->setBottom(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricBottom)));
    m_settings->setLeft(KWinScreenEdgesConfig::electricBorderActionToString(m_form->selectedEdgeItem(ElectricLeft)));

    // Save effect-specific actions:

    // Present Windows
    m_settings->setTouchBorderActivateAll(m_form->monitorCheckEffectHasEdge(PresentWindowsAll));
    m_settings->setTouchBorderActivatePresentWindows(m_form->monitorCheckEffectHasEdge(PresentWindowsCurrent));
    m_settings->setTouchBorderActivateClass(m_form->monitorCheckEffectHasEdge(PresentWindowsClass));

    // Desktop Grid
    m_settings->setTouchBorderActivateDesktopGrid(m_form->monitorCheckEffectHasEdge(DesktopGrid));

    // Desktop Cube
    m_settings->setTouchBorderActivateCube(m_form->monitorCheckEffectHasEdge(Cube));
    m_settings->setTouchBorderActivateCylinder(m_form->monitorCheckEffectHasEdge(Cylinder));
    m_settings->setTouchBorderActivateSphere(m_form->monitorCheckEffectHasEdge(Sphere));

    // TabBox
    m_settings->setTouchBorderActivateTabBox(m_form->monitorCheckEffectHasEdge(TabBox));
    m_settings->setTouchBorderAlternativeActivate(m_form->monitorCheckEffectHasEdge(TabBoxAlternative));

    // Scripts
    for (int i = 0; i < m_scripts.size(); i++) {
        int index = EffectCount + i;
        m_scriptSettings[m_scripts[i]]->setTouchBorderActivate(m_form->monitorCheckEffectHasEdge(index));
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

    // Disable Edge if TouchEdges group entries are immutable
    m_form->monitorEnableEdge(ElectricTop, !m_settings->isTopImmutable());
    m_form->monitorEnableEdge(ElectricRight, !m_settings->isRightImmutable());
    m_form->monitorEnableEdge(ElectricBottom, !m_settings->isBottomImmutable());
    m_form->monitorEnableEdge(ElectricLeft, !m_settings->isLeftImmutable());
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

#include "touch.moc"
