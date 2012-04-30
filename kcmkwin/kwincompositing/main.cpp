/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "kwin_interface.h"
#include "kwinglobals.h"

#include <kaboutdata.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <ksettings/dispatcher.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ktitlewidget.h>
#include <knotification.h>

#include <QtDBus/QtDBus>
#include <QPainter>
#include <QPaintEngine>
#include <QTimer>
#include <QLabel>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KIcon>

K_PLUGIN_FACTORY(KWinCompositingConfigFactory,
                 registerPlugin<KWin::KWinCompositingConfig>();
                )
K_EXPORT_PLUGIN(KWinCompositingConfigFactory("kcmkwincompositing"))

namespace KWin
{


ConfirmDialog::ConfirmDialog() :
    KTimerDialog(10000, KTimerDialog::CountDown, 0,
                 i18n("Confirm Desktop Effects Change"), KTimerDialog::Ok | KTimerDialog::Cancel,
                 KTimerDialog::Cancel)
{
    setObjectName(QLatin1String("mainKTimerDialog"));
    setButtonGuiItem(KDialog::Ok, KGuiItem(i18n("&Accept Configuration"), "dialog-ok"));
    setButtonGuiItem(KDialog::Cancel, KGuiItem(i18n("&Return to Previous Configuration"), "dialog-cancel"));

    QLabel *label = new QLabel(i18n("Desktop effects settings have changed.\n"
                                    "Do you want to keep the new settings?\n"
                                    "They will be automatically reverted in 10 seconds."), this);
    label->setWordWrap(true);
    setMainWidget(label);

    setWindowIcon(KIcon("preferences-desktop-effect"));
}


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QVariantList &)
    : KCModule(KWinCompositingConfigFactory::componentData(), parent)
    , mKWinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_showConfirmDialog(false)
{
    KGlobal::locale()->insertCatalog("kwin_effects");
    ui.setupUi(this);
    layout()->setMargin(0);
    ui.tabWidget->setCurrentIndex(0);
    ui.statusTitleWidget->hide();
    ui.rearmGlSupport->hide();

    // For future use
    (void) I18N_NOOP("Use GLSL shaders");

#define OPENGL_INDEX 0
#define XRENDER_INDEX 1
#ifndef KWIN_HAVE_XRENDER_COMPOSITING
    ui.compositingType->removeItem(XRENDER_INDEX);
    ui.xrenderGroup->setEnabled(false);
#define XRENDER_INDEX -1
#endif

    connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

    connect(ui.rearmGlSupportButton, SIGNAL(clicked()), this, SLOT(rearmGlSupport()));
    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.useCompositing, SIGNAL(clicked(bool)), this, SLOT(suggestGraphicsSystem()));
    connect(ui.effectWinManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectAnimations, SIGNAL(toggled(bool)), this, SLOT(changed()));

    connect(ui.effectSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(ui.effectSelector, SIGNAL(configCommitted(QByteArray)),
            this, SLOT(reparseConfiguration(QByteArray)));

    connect(ui.desktopSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.animationSpeedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(toogleSmoothScaleUi(int)));
    connect(ui.compositingType, SIGNAL(activated(int)), this, SLOT(suggestGraphicsSystem()));
    connect(ui.graphicsSystem, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.windowThumbnails, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.unredirectFullscreen , SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glScaleFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.xrScaleFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(ui.glVSync, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glShaders, SIGNAL(toggled(bool)), this, SLOT(changed()));

    // Open the temporary config file
    // Temporary conf file is used to synchronize effect checkboxes with effect
    // selector by loading/saving effects from/to temp config when active tab
    // changes.
    mTmpConfigFile.open();
    mTmpConfig = KSharedConfig::openConfig(mTmpConfigFile.fileName());

    // toggle effects shortcut button stuff - /HAS/ to happen before load!
    m_actionCollection = new KActionCollection( this, KComponentData("kwin") );
    m_actionCollection->setConfigGroup("Suspend Compositing");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = static_cast<KAction*>(m_actionCollection->addAction( "Suspend Compositing" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::ALT + Qt::SHIFT + Qt::Key_F12 ));
    connect(ui.toggleEffectsShortcut, SIGNAL(keySequenceChanged(QKeySequence)), this, SLOT(toggleEffectShortcutChanged(QKeySequence)));

    // Initialize the user interface with the config loaded from kwinrc.
    load();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
                                       ki18n("KWin Desktop Effects Configuration Module"),
                                       0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);

    // search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString slide, cube, fadedesktop;
    // desktop switcher
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_slide'");
    if (!services.isEmpty())
        slide = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cubeslide'");
    if (!services.isEmpty())
        cube = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_fadedesktop'");
    if (!services.isEmpty())
        fadedesktop = services.first()->name();

    ui.desktopSwitchingCombo->addItem(i18n("No Effect"));
    ui.desktopSwitchingCombo->addItem(slide);
    ui.desktopSwitchingCombo->addItem(cube);
    ui.desktopSwitchingCombo->addItem(fadedesktop);
}

KWinCompositingConfig::~KWinCompositingConfig()
{
}

void KWinCompositingConfig::reparseConfiguration(const QByteArray& conf)
{
    KSettings::Dispatcher::reparseConfiguration(conf);
}

void KWinCompositingConfig::showConfirmDialog(bool reinitCompositing)
{
    bool revert = false;
    // Feel free to extend this to support several kwin instances (multihead) if you
    // think it makes sense.
    OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
    if (reinitCompositing ? !kwin.compositingActive().value() : !kwin.waitForCompositingSetup().value()) {
        KMessageBox::sorry(this, i18n(
                               "Failed to activate desktop effects using the given "
                               "configuration options. Settings will be reverted to their previous values.\n\n"
                               "Check your X configuration. You may also consider changing advanced options, "
                               "especially changing the compositing type."));
        revert = true;
    } else {
        ConfirmDialog confirm;
        if (!confirm.exec())
            revert = true;
        else {
            // compositing is enabled now
            checkLoadedEffects();
        }
    }
    if (revert) {
        // Revert settings
        KConfigGroup config(mKWinConfig, "Compositing");
        config.deleteGroup();
        QMap<QString, QString>::const_iterator it = mPreviousConfig.constBegin();
        for (; it != mPreviousConfig.constEnd(); ++it) {
            if (it.value().isEmpty())
                continue;
            config.writeEntry(it.key(), it.value());
        }
        // Sync with KWin and reload
        configChanged(reinitCompositing);
        load();
    }
}

void KWinCompositingConfig::initEffectSelector()
{
    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Appearance"), "Appearance", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Accessibility"), "Accessibility", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Focus"), "Focus", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Candy"), "Candy", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tools"), "Tools", mTmpConfig);
}

void KWinCompositingConfig::currentTabChanged(int tab)
{
    // block signals to don't emit the changed()-signal by just switching the current tab
    blockSignals(true);

    // write possible changes done to synchronize effect checkboxes and selector
    // TODO: This segment is prone to fail when the UI is changed;
    // you'll most likely not think of the hard coded numbers here when just changing the order of the tabs.
    if (tab == 0) {
        // General tab was activated
        saveEffectsTab();
        loadGeneralTab();
    } else if (tab == 1) {
        // Effects tab was activated
        saveGeneralTab();
        loadEffectsTab();
    }

    blockSignals(false);
}

void KWinCompositingConfig::loadGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    bool enabled = config.readEntry("Enabled", true);
    ui.useCompositing->setChecked(enabled);

    // this works by global shortcut magics - it will pick the current sc
    // but the constructor line that adds the default alt+shift+f12 gsc is IMPORTANT!
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("Suspend Compositing")))
        ui.toggleEffectsShortcut->setKeySequence(a->globalShortcut().primary());

    ui.animationSpeedCombo->setCurrentIndex(config.readEntry("AnimationSpeed", 3));

    // Load effect settings
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  effectconfig.readEntry("kwin4_effect_" effectname "Enabled", true)
    int winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows")
                               + LOAD_EFFECT_CONFIG("desktopgrid")
                               + LOAD_EFFECT_CONFIG("dialogparent");
    if (winManagementEnabled > 0 && winManagementEnabled < 3) {
        ui.effectWinManagement->setTristate(true);
        ui.effectWinManagement->setCheckState(Qt::PartiallyChecked);
    } else
        ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));
#undef LOAD_EFFECT_CONFIG

    // desktop switching
    // Set current option to "none" if no plugin is activated.
    ui.desktopSwitchingCombo->setCurrentIndex(0);
    if (effectEnabled("slide", effectconfig))
        ui.desktopSwitchingCombo->setCurrentIndex(1);
    if (effectEnabled("cubeslide", effectconfig))
        ui.desktopSwitchingCombo->setCurrentIndex(2);
    if (effectEnabled("fadedesktop", effectconfig))
        ui.desktopSwitchingCombo->setCurrentIndex(3);
}

void KWinCompositingConfig::rearmGlSupport()
{
    // rearm config
    KConfigGroup gl_workaround_config = KConfigGroup(mKWinConfig, "Compositing");
    gl_workaround_config.writeEntry("OpenGLIsUnsafe", false);
    gl_workaround_config.sync();

    // save last changes
    save();

    // Initialize the user interface with the config loaded from kwinrc.
    load();
}

void KWinCompositingConfig::suggestGraphicsSystem()
{
    if (!ui.useCompositing->isChecked() || ui.compositingType->currentIndex() == XRENDER_INDEX)
        ui.graphicsSystem->setCurrentIndex(0);
}


void KWinCompositingConfig::toogleSmoothScaleUi(int compositingType)
{
    ui.glScaleFilter->setVisible(compositingType == OPENGL_INDEX);
    ui.xrScaleFilter->setVisible(compositingType == XRENDER_INDEX);
    ui.scaleMethodLabel->setBuddy(compositingType == XRENDER_INDEX ? ui.xrScaleFilter : ui.glScaleFilter);
    ui.glGroup->setEnabled(compositingType == OPENGL_INDEX);
}

void KWinCompositingConfig::toggleEffectShortcutChanged(const QKeySequence &seq)
{
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("Suspend Compositing")))
        a->setGlobalShortcut(KShortcut(seq), KAction::ActiveShortcut, KAction::NoAutoloading);
    m_actionCollection->writeSettings();
}

bool KWinCompositingConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

void KWinCompositingConfig::loadEffectsTab()
{
    ui.effectSelector->load();
}

void KWinCompositingConfig::loadAdvancedTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? XRENDER_INDEX : 0);

    originalGraphicsSystem = config.readEntry("GraphicsSystem", QString());
    if (originalGraphicsSystem.isEmpty()) { // detect system default
        QPixmap pix(1,1);
        QPainter p(&pix);
        originalGraphicsSystem = (p.paintEngine()->type() == QPaintEngine::X11) ? "native" : "raster";
        p.end();
    }
    ui.graphicsSystem->setCurrentIndex((originalGraphicsSystem == "native") ? 0 : 1);

    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if (hps == 6)   // always
        ui.windowThumbnails->setCurrentIndex(0);
    else if (hps == 4)   // never
        ui.windowThumbnails->setCurrentIndex(2);
    else // shown, or default
        ui.windowThumbnails->setCurrentIndex(1);
    ui.unredirectFullscreen->setChecked(config.readEntry("UnredirectFullscreen", false));

    ui.xrScaleFilter->setCurrentIndex((int)config.readEntry("XRenderSmoothScale", false));
    ui.glScaleFilter->setCurrentIndex(config.readEntry("GLTextureFilter", 2));

    ui.glVSync->setChecked(config.readEntry("GLVSync", true));
    ui.glShaders->setChecked(!config.readEntry<bool>("GLLegacy", false));

    toogleSmoothScaleUi(ui.compositingType->currentIndex());
}

void KWinCompositingConfig::updateStatusUI(bool compositingIsPossible)
{
    if (compositingIsPossible) {
        ui.compositingOptionsContainer->show();
        ui.statusTitleWidget->hide();
        ui.rearmGlSupport->hide();
    }
    else {
        OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
        ui.compositingOptionsContainer->hide();
        QString text = i18n("Desktop effects are not available on this system due to the following technical issues:");
        text += "<hr>";
        text += kwin.isValid() ? kwin.compositingNotPossibleReason() : i18nc("Reason shown when trying to activate desktop effects and KWin (most likely) crashes",
                                                                             "Window Manager seems not to be running");
        ui.statusTitleWidget->setText(text);
        ui.statusTitleWidget->setPixmap(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
        ui.statusTitleWidget->show();
        ui.rearmGlSupport->setVisible(kwin.isValid() ? kwin.openGLIsBroken() : true);
    }
}

void KWinCompositingConfig::load()
{
    initEffectSelector();
    mKWinConfig->reparseConfiguration();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "compositingPossible");
    QDBusConnection::sessionBus().callWithCallback(msg, this, SLOT(updateStatusUI(bool)));

    // Copy Plugins group to temp config file
    QMap<QString, QString> entries = mKWinConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup tmpconfig(mTmpConfig, "Plugins");
    tmpconfig.deleteGroup();
    for (; it != entries.constEnd(); ++it)
        tmpconfig.writeEntry(it.key(), it.value());

    loadGeneralTab();
    loadEffectsTab();
    loadAdvancedTab();

    emit changed(false);
}

void KWinCompositingConfig::saveGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    // Check if any critical settings that need confirmation have changed
    config.writeEntry("Enabled", ui.useCompositing->isChecked());
    config.writeEntry("AnimationSpeed", ui.animationSpeedCombo->currentIndex());

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    if (ui.effectWinManagement->checkState() != Qt::PartiallyChecked) {
        WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    }
    // TODO: maybe also do some effect-specific configuration here, e.g.
    //  enable/disable desktopgrid's animation according to this setting
    WRITE_EFFECT_CONFIG("minimizeanimation", ui.effectAnimations);
#undef WRITE_EFFECT_CONFIG

    int desktopSwitcher = ui.desktopSwitchingCombo->currentIndex();
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
}

void KWinCompositingConfig::saveEffectsTab()
{
    ui.effectSelector->save();
}

bool KWinCompositingConfig::saveAdvancedTab()
{
    bool advancedChanged = false;
    static const int hps[] = { 6 /*always*/, 5 /*shown*/,  4 /*never*/ };

    KConfigGroup config(mKWinConfig, "Compositing");
    QString graphicsSystem = (ui.graphicsSystem->currentIndex() == 0) ? "native" : "raster";

    if (config.readEntry("Backend", "OpenGL")
            != ((ui.compositingType->currentIndex() == OPENGL_INDEX) ? "OpenGL" : "XRender")
            || config.readEntry("GLVSync", true) != ui.glVSync->isChecked()
            || config.readEntry<bool>("GLLegacy", false) == ui.glShaders->isChecked()) {
        m_showConfirmDialog = true;
        advancedChanged = true;
    } else if (config.readEntry("HiddenPreviews", 5) != hps[ ui.windowThumbnails->currentIndex()]
              || (int)config.readEntry("XRenderSmoothScale", false) != ui.xrScaleFilter->currentIndex()
              || config.readEntry("GLTextureFilter", 2) != ui.glScaleFilter->currentIndex()) {
        advancedChanged = true;
    } else if (originalGraphicsSystem != graphicsSystem) {
        advancedChanged = true;
    }

    config.writeEntry("Backend", (ui.compositingType->currentIndex() == OPENGL_INDEX) ? "OpenGL" : "XRender");
    config.writeEntry("GraphicsSystem", graphicsSystem);
    config.writeEntry("HiddenPreviews", hps[ ui.windowThumbnails->currentIndex()]);
    config.writeEntry("UnredirectFullscreen", ui.unredirectFullscreen->isChecked());

    config.writeEntry("XRenderSmoothScale", ui.xrScaleFilter->currentIndex() == 1);
    config.writeEntry("GLTextureFilter", ui.glScaleFilter->currentIndex());

    config.writeEntry("GLVSync", ui.glVSync->isChecked());
    config.writeEntry("GLLegacy", !ui.glShaders->isChecked());


    return advancedChanged;
}

void KWinCompositingConfig::save()
{
    OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
    if (ui.compositingType->currentIndex() == OPENGL_INDEX &&
        kwin.openGLIsBroken() && !ui.rearmGlSupport->isVisible())
    {
        KConfigGroup config(mKWinConfig, "Compositing");
        QString oldBackend = config.readEntry("Backend", "OpenGL");
        config.writeEntry("Backend", "OpenGL");
        config.sync();
        updateStatusUI(false);
        config.writeEntry("Backend", oldBackend);
        config.sync();
        ui.tabWidget->setCurrentIndex(0);
        return;
    }

    // Save current config. We'll use this for restoring in case something goes wrong.
    KConfigGroup config(mKWinConfig, "Compositing");
    mPreviousConfig = config.entryMap();

    // bah; tab content being dependent on the other is really bad; and
    // deprecated in the HIG for a reason.  It is confusing!
    // Make sure we only call save on each tab once; as they are stateful due to the revert concept
    if (ui.tabWidget->currentIndex() == 0) {  // "General" tab was active
        saveGeneralTab();
        loadEffectsTab();
        saveEffectsTab();
    } else {
        saveEffectsTab();
        loadGeneralTab();
        saveGeneralTab();
    }
    bool advancedChanged = saveAdvancedTab();

    // Copy Plugins group from temp config to real config
    QMap<QString, QString> entries = mTmpConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup realconfig(mKWinConfig, "Plugins");
    realconfig.deleteGroup();
    for (; it != entries.constEnd(); ++it)
        realconfig.writeEntry(it.key(), it.value());

    emit changed(false);

    configChanged(advancedChanged);

    // This assumes that this KCM is running with the same environment variables as KWin
    // TODO: Detect KWIN_COMPOSE=N as well
    if (!qgetenv("KDE_FAILSAFE").isNull() && ui.useCompositing->isChecked()) {
        KMessageBox::sorry(this, i18n(
                               "Your settings have been saved but as KDE is currently running in failsafe "
                               "mode desktop effects cannot be enabled at this time.\n\n"
                               "Please exit failsafe mode to enable desktop effects."));
        m_showConfirmDialog = false; // Dangerous but there is no way to test if failsafe mode
    }

    if (m_showConfirmDialog) {
        m_showConfirmDialog = false;
        if (advancedChanged)
            QTimer::singleShot(1000, this, SLOT(confirmReInit()));
        else
            showConfirmDialog(false);
    }
}

void KWinCompositingConfig::checkLoadedEffects()
{
    // check for effects not supported by Backend or hardware
    // such effects are enabled but not returned by DBus method loadedEffects
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "loadedEffects");
    QDBusMessage reply = QDBusConnection::sessionBus().call(message);
    KConfigGroup effectConfig = KConfigGroup(mKWinConfig, "Compositing");
    bool enabledAfter = effectConfig.readEntry("Enabled", true);

    if (reply.type() == QDBusMessage::ReplyMessage && enabledAfter && !getenv("KDE_FAILSAFE")) {
        effectConfig = KConfigGroup(mKWinConfig, "Plugins");
        QStringList loadedEffects = reply.arguments()[0].toStringList();
        QStringList effects = effectConfig.keyList();
        QStringList disabledEffects = QStringList();
        foreach (QString effect, effects) { // krazy:exclude=foreach
            QString temp = effect.mid(13, effect.length() - 13 - 7);
            effect.truncate(effect.length() - 7);
            if (effectEnabled(temp, effectConfig) && !loadedEffects.contains(effect)) {
                disabledEffects << effect;
            }
        }
        if (!disabledEffects.isEmpty()) {
            KServiceTypeTrader* trader = KServiceTypeTrader::self();
            KService::List services;
            QString message = i18n("The following desktop effects could not be activated:");
            message.append("<ul>");
            foreach (const QString & effect, disabledEffects) {
                services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == '" + effect + '\'');
                message.append("<li>");
                if (!services.isEmpty())
                    message.append(services.first()->name());
                else
                    message.append(effect);
                message.append("</li>");
            }
            message.append("</ul>");
            KNotification::event("effectsnotsupported", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData("kwin"));
        }
    }
}

void KWinCompositingConfig::configChanged(bool reinitCompositing)
{
    // Send signal to kwin
    mKWinConfig->sync();

    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin",
                           reinitCompositing ? "reinitCompositing" : "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    // maybe it's ok now?
    if (reinitCompositing && !ui.compositingOptionsContainer->isVisible())
        load();

    // HACK: We can't just do this here, due to the asynchronous nature of signals.
    // We also can't change reinitCompositing into a message (which would allow
    // callWithCallbac() to do this neater) due to multiple kwin instances.
    if (!m_showConfirmDialog)
        QTimer::singleShot(1000, this, SLOT(checkLoadedEffects()));
}


void KWinCompositingConfig::defaults()
{
    ui.tabWidget->setCurrentIndex(0);

    ui.useCompositing->setChecked(true);
    ui.effectWinManagement->setChecked(true);
    ui.effectAnimations->setChecked(true);

    ui.desktopSwitchingCombo->setCurrentIndex(1);
    ui.animationSpeedCombo->setCurrentIndex(3);

    ui.effectSelector->defaults();

    ui.compositingType->setCurrentIndex(0);
    ui.windowThumbnails->setCurrentIndex(1);
    ui.unredirectFullscreen->setChecked(false);
    ui.xrScaleFilter->setCurrentIndex(0);
    ui.glScaleFilter->setCurrentIndex(2);
    ui.glVSync->setChecked(true);
    ui.glShaders->setChecked(true);
}

QString KWinCompositingConfig::quickHelp() const
{
    return i18n("<h1>Desktop Effects</h1>");
}

} // namespace

#include "main.moc"
