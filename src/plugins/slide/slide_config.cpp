/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "slide_config.h"

#include "config-kwin.h"

// KConfigSkeleton
#include "slideconfig.h"

#include <kwineffects_interface.h>

#include <KPluginFactory>

K_PLUGIN_CLASS(KWin::SlideEffectConfig)

namespace KWin
{

SlideEffectConfig::SlideEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    SlideConfig::instance(KWIN_CONFIG);
    addConfig(SlideConfig::self(), widget());

    // The gap is the cleared framebuffer showing through, so it is only ever
    // visible while the background slides along with the desktops. Left
    // stationary the wallpaper covers the output at all times and the color
    // has nothing to show through, so follow the checkbox rather than leaving
    // a control that silently does nothing.
    //
    // The initial state is applied here rather than through a connection in
    // the .ui file: that would only react to a change, and loading a stored
    // false into an already unchecked box emits nothing.
    auto followSlideBackground = [this](bool enabled) {
        m_ui.label_GapColor->setEnabled(enabled);
        m_ui.kcfg_GapColor->setEnabled(enabled);
    };
    connect(m_ui.kcfg_SlideBackground, &QCheckBox::toggled, this, followSlideBackground);
    followSlideBackground(m_ui.kcfg_SlideBackground->isChecked());
}

SlideEffectConfig::~SlideEffectConfig()
{
}

void SlideEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("slide"));
}

} // namespace KWin

#include "slide_config.moc"

#include "moc_slide_config.cpp"
