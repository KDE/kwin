/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorblindnesscorrection.h"

#include <KSharedConfig>

#include "libkwineffects/kwineffects.h"
#include "opengl/glshader.h"

#include "colorblindnesscorrection_settings.h"

Q_LOGGING_CATEGORY(KWIN_COLORBLINDNESS_CORRECTION, "kwin_effect_colorblindnesscorrection", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(colorblindnesscorrection);
}

namespace KWin
{

ColorBlindnessCorrectionEffect::ColorBlindnessCorrectionEffect()
    : OffscreenEffect()
    , m_mode(static_cast<Mode>(ColorBlindnessCorrectionSettings().mode()))
{
    loadData();
}

ColorBlindnessCorrectionEffect::~ColorBlindnessCorrectionEffect()
{
}

bool ColorBlindnessCorrectionEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void ColorBlindnessCorrectionEffect::loadData()
{
    ensureResources();

    QString fragPath;
    switch (m_mode) {
    case Deuteranopia:
        fragPath = QStringLiteral(":/effects/colorblindnesscorrection/shaders/Deutranopia.frag");
        break;
    case Tritanopia:
        fragPath = QStringLiteral(":/effects/colorblindnesscorrection/shaders/Tritanopia.frag");
        break;
    case Protanopia: // Most common, use it as fallback
    default:
        fragPath = QStringLiteral(":/effects/colorblindnesscorrection/shaders/Protanopia.frag");
        break;
    }

    m_shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), fragPath);

    if (!m_shader->isValid()) {
        qCCritical(KWIN_COLORBLINDNESS_CORRECTION) << "Failed to load the shader!";
        return;
    }

    for (const auto windows = effects->stackingOrder(); EffectWindow * w : windows) {
        correctColor(w);
    }
    effects->addRepaintFull();

    connect(effects, &EffectsHandler::windowDeleted, this, &ColorBlindnessCorrectionEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowAdded, this, &ColorBlindnessCorrectionEffect::correctColor);
}

void ColorBlindnessCorrectionEffect::correctColor(KWin::EffectWindow *w)
{
    if (m_windows.contains(w)) {
        return;
    }

    redirect(w);
    setShader(w, m_shader.get());
    m_windows.insert(w);
}

void ColorBlindnessCorrectionEffect::slotWindowDeleted(EffectWindow *w)
{
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        m_windows.erase(it);
    }
}

bool ColorBlindnessCorrectionEffect::isActive() const
{
    return !m_windows.empty();
}

bool ColorBlindnessCorrectionEffect::provides(Feature f)
{
    return f == Contrast;
}

void ColorBlindnessCorrectionEffect::reconfigure(ReconfigureFlags flags)
{
    if (flags != Effect::ReconfigureAll) {
        return;
    }

    auto newMode = static_cast<Mode>(ColorBlindnessCorrectionSettings().mode());
    if (m_mode == newMode) {
        return;
    }

    m_mode = newMode;

    disconnect(effects, &EffectsHandler::windowDeleted, this, &ColorBlindnessCorrectionEffect::slotWindowDeleted);
    disconnect(effects, &EffectsHandler::windowAdded, this, &ColorBlindnessCorrectionEffect::correctColor);

    for (EffectWindow *w : m_windows) {
        unredirect(w);
    }
    m_windows.clear();

    loadData();
}

int ColorBlindnessCorrectionEffect::requestedEffectChainPosition() const
{
    return 98;
}

} // namespace

#include "moc_colorblindnesscorrection.cpp"
