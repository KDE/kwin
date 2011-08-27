/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "invert.h"

#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>
#include <KStandardDirs>

#include <QMatrix4x4>

namespace KWin
{

KWIN_EFFECT(invert, InvertEffect)
KWIN_EFFECT_SUPPORTED(invert, InvertEffect::supported())

InvertEffect::InvertEffect()
    :   m_inited(false),
        m_valid(true),
        m_shader(NULL),
        m_allWindows(false)
{
    KActionCollection* actionCollection = new KActionCollection(this);

    KAction* a = (KAction*)actionCollection->addAction("Invert");
    a->setText(i18n("Toggle Invert Effect"));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_I));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));

    KAction* b = (KAction*)actionCollection->addAction("InvertWindow");
    b->setText(i18n("Toggle Invert Effect on Window"));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_U));
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleWindow()));
    connect(effects, SIGNAL(windowClosed(EffectWindow*)), this, SLOT(slotWindowClosed(EffectWindow*)));
}

InvertEffect::~InvertEffect()
{
    delete m_shader;
}

bool InvertEffect::supported()
{
    return GLPlatform::instance()->supports(GLSL) &&
           (effects->compositingType() == OpenGLCompositing);
}

bool InvertEffect::loadData()
{
    m_inited = true;
    if (!ShaderManager::instance()->isValid()) {
        return false;
    }

    const QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/invert.frag");

    m_shader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, fragmentshader);
    if (!m_shader->isValid()) {
        kError(1212) << "The shader failed to load!" << endl;
        return false;
    }

    return true;
}

void InvertEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (m_valid && (m_allWindows || !m_windows.isEmpty())) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS;
    }
    effects->prePaintScreen(data, time);
}

void InvertEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_valid && (m_allWindows != m_windows.contains(w))) {
        data.mask |= PAINT_WINDOW_TRANSFORMED;
    }
    effects->prePaintWindow(w, data, time);
}

void InvertEffect::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    // Load if we haven't already
    if (m_valid && !m_inited)
        m_valid = loadData();

    bool useShader = m_valid && (m_allWindows != m_windows.contains(w));
    if (useShader) {
        ShaderManager *shaderManager = ShaderManager::instance();
        GLShader *genericShader = shaderManager->pushShader(ShaderManager::GenericShader);
        QMatrix4x4 screenTransformation = genericShader->getUniformMatrix4x4("screenTransformation");
        shaderManager->popShader();
        shaderManager->pushShader(m_shader);
        m_shader->setUniform("screenTransformation", screenTransformation);

        data.shader = m_shader;
    }

    effects->drawWindow(w, mask, region, data);

    if (useShader) {
        ShaderManager::instance()->popShader();
    }
}

void InvertEffect::paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (m_valid && m_allWindows) {
        frame->setShader(m_shader);
        ShaderManager::instance()->pushShader(m_shader);
        m_shader->setUniform("screenTransformation", QMatrix4x4());
        m_shader->setUniform("windowTransformation", QMatrix4x4());
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
        ShaderManager::instance()->popShader();
    } else {
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    }
}

void InvertEffect::slotWindowClosed(EffectWindow* w)
{
    m_windows.removeOne(w);
}

void InvertEffect::toggle()
{
    m_allWindows = !m_allWindows;
    effects->addRepaintFull();
}

void InvertEffect::toggleWindow()
{
    if (!m_windows.contains(effects->activeWindow()))
        m_windows.append(effects->activeWindow());
    else
        m_windows.removeOne(effects->activeWindow());
    effects->activeWindow()->addRepaintFull();
}

bool InvertEffect::isActive() const
{
    return m_valid && (m_allWindows || !m_windows.isEmpty());
}

} // namespace

#include "invert.moc"
