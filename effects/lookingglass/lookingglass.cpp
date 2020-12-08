/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lookingglass.h"

// KConfigSkeleton
#include "lookingglassconfig.h"

#include <QAction>
#include <kwinglutils.h>
#include <kwinglplatform.h>

#include <KStandardAction>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QVector2D>
#include <QFile>

#include <kmessagebox.h>

#include <cmath>

namespace KWin
{

LookingGlassEffect::LookingGlassEffect()
    : zoom(1.0f)
    , target_zoom(1.0f)
    , polling(false)
    , m_texture(nullptr)
    , m_fbo(nullptr)
    , m_vbo(nullptr)
    , m_shader(nullptr)
    , m_enabled(false)
    , m_valid(false)
{
    initConfig<LookingGlassConfig>();
    QAction* a;
    a = KStandardAction::zoomIn(this, SLOT(zoomIn()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Equal, a);

    a = KStandardAction::zoomOut(this, SLOT(zoomOut()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Minus, a);

    a = KStandardAction::actualSize(this, SLOT(toggle()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_0, a);

    connect(effects, &EffectsHandler::mouseChanged, this, &LookingGlassEffect::slotMouseChanged);

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
    return effects->compositingType() == OpenGL2Compositing && !GLPlatform::instance()->supports(LimitedNPOT);
}

void LookingGlassEffect::reconfigure(ReconfigureFlags)
{
    LookingGlassConfig::self()->read();
    initialradius = LookingGlassConfig::radius();
    radius = initialradius;
    qCDebug(KWINEFFECTS) << "Radius from config:" << radius;
    m_valid = loadData();
}

bool LookingGlassEffect::loadData()
{
    const QSize screenSize = effects->virtualScreenSize();
    int texw = screenSize.width();
    int texh = screenSize.height();

    // Create texture and render target
    const int levels = std::log2(qMin(texw, texh)) + 1;
    m_texture = new GLTexture(GL_RGBA8, texw, texh, levels);
    m_texture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_fbo = new GLRenderTarget(*m_texture);
    if (!m_fbo->valid()) {
        return false;
    }

    m_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("lookingglass.frag"));
    if (m_shader->isValid()) {
        ShaderBinder binder(m_shader);
        m_shader->setUniform("u_textureSize", QVector2D(screenSize.width(), screenSize.height()));
    } else {
        qCCritical(KWINEFFECTS) << "The shader failed to load!";
        return false;
    }

    m_vbo = new GLVertexBuffer(GLVertexBuffer::Static);
    QVector<float> verts;
    QVector<float> texcoords;
    texcoords << screenSize.width() << 0.0;
    verts << screenSize.width() << 0.0;
    texcoords << 0.0 << 0.0;
    verts << 0.0 << 0.0;
    texcoords << 0.0 << screenSize.height();
    verts << 0.0 << screenSize.height();
    texcoords << 0.0 << screenSize.height();
    verts << 0.0 << screenSize.height();
    texcoords << screenSize.width() << screenSize.height();
    verts << screenSize.width() << screenSize.height();
    texcoords << screenSize.width() << 0.0;
    verts << screenSize.width() << 0.0;
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
        qCDebug(KWINEFFECTS) << "zoom is now " << zoom;
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

void LookingGlassEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    // Call the next effect.
    effects->paintScreen(mask, region, data);
    if (m_valid && m_enabled) {
        // Disable render texture
        GLRenderTarget* target = GLRenderTarget::popRenderTarget();
        Q_ASSERT(target == m_fbo);
        Q_UNUSED(target);
        m_texture->bind();
        m_texture->generateMipmaps();

        // Use the shader
        ShaderBinder binder(m_shader);
        m_shader->setUniform("u_zoom", (float)zoom);
        m_shader->setUniform("u_radius", (float)radius);
        m_shader->setUniform("u_cursor", QVector2D(cursorPos().x(), cursorPos().y()));
        m_shader->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
        m_vbo->render(GL_TRIANGLES);
        m_texture->unbind();
    }
}

bool LookingGlassEffect::isActive() const
{
    return m_valid && m_enabled;
}

} // namespace

