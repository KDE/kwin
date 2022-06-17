/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "debugeffect.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QFile>
#include <QStandardPaths>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QMatrix4x4>

Q_LOGGING_CATEGORY(KWIN_DEBUG, "kwin_effect_debug", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(debugeffect);
}

namespace KWin
{

DebugEffect::DebugEffect()
{
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("Debug"));
    a->setText(i18n("Toggle Debugging Fractional Scaling"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_F));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::CTRL | Qt::META | Qt::Key_F));
    effects->registerGlobalShortcut(Qt::CTRL | Qt::META | Qt::Key_F, a);
    connect(a, &QAction::triggered, this, &DebugEffect::toggleDebugFractional);
}

DebugEffect::~DebugEffect()
{
}

bool DebugEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

bool DebugEffect::loadData()
{
    ensureResources();
    m_inited = true;

    m_debugFractionalShader = std::unique_ptr<GLShader>(ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QStringLiteral(":/effects/debug/shaders/debug_fractional.vert"), QStringLiteral(":/effects/debug/shaders/debug_fractional.frag")));
    if (!m_debugFractionalShader->isValid()) {
        qCCritical(KWIN_DEBUG) << "The fractional debug shader failed to load!";
        return false;
    }

    return true;
}

void DebugEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    // Load if we haven't already
    if (m_valid && !m_inited) {
        m_valid = loadData();
    }

    bool useFractionalShader = m_valid && m_debugFractionalEnabled;
    if (useFractionalShader) {
        ShaderManager *shaderManager = ShaderManager::instance();
        shaderManager->pushShader(m_debugFractionalShader.get());
        data.shader = m_debugFractionalShader.get();

        m_debugFractionalShader->setUniform("fractionalPrecision", 0.01f);

        auto dpr = w->screen()->devicePixelRatio();

        auto screenSize = w->screen()->geometry().size() * dpr;
        m_debugFractionalShader->setUniform("screenSize", QVector2D{float(screenSize.width()), float(screenSize.height())});

        auto fg = QVector2D{
            float(w->frameGeometry().width() * dpr),
            float(w->frameGeometry().height() * dpr)};
        m_debugFractionalShader->setUniform("geometrySize", fg);
    }

    effects->drawWindow(w, mask, region, data);

    if (useFractionalShader) {
        ShaderManager::instance()->popShader();
    }
}

void DebugEffect::toggleDebugFractional()
{
    m_debugFractionalEnabled = !m_debugFractionalEnabled;
    effects->addRepaintFull();
}

bool DebugEffect::isActive() const
{
    return m_valid && m_debugFractionalEnabled;
}

} // namespace
