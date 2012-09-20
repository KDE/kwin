/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "lookingglass.h"

// KConfigSkeleton
#include "lookingglassconfig.h"

#include <kwinglutils.h>
#include <kwinglplatform.h>

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>
#include <KDE/KGlobal>
#include <KDE/KStandardDirs>
#include <QVector2D>

#include <kmessagebox.h>

namespace KWin
{

KWIN_EFFECT(lookingglass, LookingGlassEffect)
KWIN_EFFECT_SUPPORTED(lookingglass, LookingGlassEffect::supported())


LookingGlassEffect::LookingGlassEffect()
    : zoom(1.0f)
    , target_zoom(1.0f)
    , polling(false)
    , m_texture(NULL)
    , m_fbo(NULL)
    , m_vbo(NULL)
    , m_shader(NULL)
    , m_enabled(false)
    , m_valid(false)
{
    actionCollection = new KActionCollection(this);
    actionCollection->setConfigGlobal(true);
    actionCollection->setConfigGroup("LookingGlass");

    KAction* a;
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomIn, this, SLOT(zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Plus));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomOut, this, SLOT(zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ActualSize, this, SLOT(toggle())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

LookingGlassEffect::~LookingGlassEffect()
{
    delete m_texture;
    delete m_fbo;
    delete m_shader;
    delete m_vbo;
}

bool LookingGlassEffect::supported()
{
    return effects->compositingType() == OpenGL2Compositing;
}

void LookingGlassEffect::reconfigure(ReconfigureFlags)
{
    LookingGlassConfig::self()->readConfig();
    initialradius = LookingGlassConfig::radius();
    radius = initialradius;
    kDebug(1212) << QString("Radius from config: %1").arg(radius) << endl;
    actionCollection->readSettings();
    m_valid = loadData();
}

bool LookingGlassEffect::loadData()
{
    // If NPOT textures are not supported, use nearest power-of-two sized
    //  texture. It wastes memory, but it's possible to support systems without
    //  NPOT textures that way
    int texw = displayWidth();
    int texh = displayHeight();
    if (!GLTexture::NPOTTextureSupported()) {
        kWarning(1212) << "NPOT textures not supported, wasting some memory" ;
        texw = nearestPowerOfTwo(texw);
        texh = nearestPowerOfTwo(texh);
    }
    // Create texture and render target
    m_texture = new GLTexture(texw, texh);
    m_texture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_fbo = new GLRenderTarget(*m_texture);
    if (!m_fbo->valid()) {
        return false;
    }

    const QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/lookingglass.frag");
    m_shader = ShaderManager::instance()->loadFragmentShader(ShaderManager::SimpleShader, fragmentshader);
    if (m_shader->isValid()) {
        ShaderManager::instance()->pushShader(m_shader);
        m_shader->setUniform("u_textureSize", QVector2D(displayWidth(), displayHeight()));
        ShaderManager::instance()->popShader();
    } else {
        kError(1212) << "The shader failed to load!" << endl;
        return false;
    }

    m_vbo = new GLVertexBuffer(GLVertexBuffer::Static);
    QVector<float> verts;
    QVector<float> texcoords;
    texcoords << displayWidth() << 0.0;
    verts << displayWidth() << 0.0;
    texcoords << 0.0 << 0.0;
    verts << 0.0 << 0.0;
    texcoords << 0.0 << displayHeight();
    verts << 0.0 << displayHeight();
    texcoords << 0.0 << displayHeight();
    verts << 0.0 << displayHeight();
    texcoords << displayWidth() << displayHeight();
    verts << displayWidth() << displayHeight();
    texcoords << displayWidth() << 0.0;
    verts << displayWidth() << 0.0;
    m_vbo->setData(6, 2, verts.constData(), texcoords.constData());
    return true;
}

void LookingGlassEffect::toggle()
{
    if (target_zoom == 1.0f) {
        target_zoom = 2.0f;
        if (!polling) {
            polling = true;
            effects->startMousePolling();
        }
        m_enabled = true;
    } else {
        target_zoom = 1.0f;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
        if (zoom == target_zoom) {
            m_enabled = false;
        }
    }
    effects->addRepaint(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
}

void LookingGlassEffect::zoomIn()
{
    target_zoom = qMin(7.0, target_zoom + 0.5);
    m_enabled = true;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    effects->addRepaint(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
}

void LookingGlassEffect::zoomOut()
{
    target_zoom -= 0.5;
    if (target_zoom < 1) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
        if (zoom == target_zoom) {
            m_enabled = false;
        }
    }
    effects->addRepaint(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
}

void LookingGlassEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (zoom != target_zoom) {
        double diff = time / animationTime(500.0);
        if (target_zoom > zoom)
            zoom = qMin(zoom * qMax(1.0 + diff, 1.2), target_zoom);
        else
            zoom = qMax(zoom * qMin(1.0 - diff, 0.8), target_zoom);
        kDebug(1212) << "zoom is now " << zoom;
        radius = qBound((double)initialradius, initialradius * zoom, 3.5 * initialradius);

        if (zoom <= 1.0f) {
            m_enabled = false;
        }

        effects->addRepaint(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
    }
    if (m_valid && m_enabled) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        GLRenderTarget::pushRenderTarget(m_fbo);
    }

    effects->prePaintScreen(data, time);
}

void LookingGlassEffect::slotMouseChanged(const QPoint& pos, const QPoint& old, Qt::MouseButtons,
                                      Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (pos != old && m_enabled) {
        effects->addRepaint(pos.x() - radius, pos.y() - radius, 2 * radius, 2 * radius);
        effects->addRepaint(old.x() - radius, old.y() - radius, 2 * radius, 2 * radius);
    }
}

void LookingGlassEffect::postPaintScreen()
{
    // Call the next effect.
    effects->postPaintScreen();
    if (m_valid && m_enabled) {
        // Disable render texture
        GLRenderTarget* target = GLRenderTarget::popRenderTarget();
        assert(target == m_fbo);
        Q_UNUSED(target);
        m_texture->bind();

        // Use the shader
        ShaderManager::instance()->pushShader(m_shader);
        m_shader->setUniform("u_zoom", (float)zoom);
        m_shader->setUniform("u_radius", (float)radius);
        m_shader->setUniform("u_cursor", QVector2D(cursorPos().x(), cursorPos().y()));
        m_vbo->render(GL_TRIANGLES);
        ShaderManager::instance()->popShader();
        m_texture->unbind();
    }
}

bool LookingGlassEffect::isActive() const
{
    return m_valid && m_enabled;
}

} // namespace

#include "lookingglass.moc"
