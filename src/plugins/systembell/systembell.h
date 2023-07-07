/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/offscreeneffect.h"

#include <KConfigGroup>
#include <KConfigWatcher>

class ca_context;

namespace KWin
{

class GLShader;
class XdgSystemBellV1Interface;

class SystemBellEffect : public OffscreenEffect
{
    Q_OBJECT
public:
    SystemBellEffect();
    ~SystemBellEffect() override;

    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    bool provides(Feature f) override;
    bool perform(Feature feature, const QVariantList &arguments) override;
    void reconfigure(ReconfigureFlags flags) override;

    static bool supported();

public Q_SLOTS:
    void triggerScreen();
    void triggerWindow();

private Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    bool loadData();

private:
    enum Mode {
        Invert,
        Color,
    };

    void triggerWindow(EffectWindow *window);
    void flash(EffectWindow *window);
    void unflash(EffectWindow *window);
    void loadConfig(const KConfigGroup &group);
    void playAudibleBell();

    bool m_inited = false;
    bool m_valid = true;
    std::unique_ptr<GLShader> m_shader;
    bool m_allWindows = false;
    QList<EffectWindow *> m_windows;
    QColor m_color;
    int m_duration;
    Mode m_mode;
    ca_context *m_caContext = nullptr;
    bool m_visibleBell = false;
    bool m_audibleBell = false;
    bool m_customBell = false;
    QString m_customBellFile;
    KConfigWatcher::Ptr m_configWatcher;
    KConfig m_kdeglobals;
    KConfigGroup m_bellConfig;

    static QTimer *s_systemBellRemoveTimer;
    static XdgSystemBellV1Interface *s_systemBell;
};

inline int SystemBellEffect::requestedEffectChainPosition() const
{
    return 99;
}

} // namespace
