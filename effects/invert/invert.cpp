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

#include <QAction>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QStandardPaths>

#include <QMatrix4x4>

namespace KWin
{

InvertEffect::InvertEffect()
    :   m_inited(false),
        m_valid(true),
        m_shader(NULL),
        m_allWindows(false)
{
    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("Invert"));
    a->setText(i18n("Toggle Invert Effect"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_I, a);
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleScreenInversion()));

    QAction* b = new QAction(this);
    b->setObjectName(QStringLiteral("InvertWindow"));
    b->setText(i18n("Toggle Invert Effect on Window"));
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_U, b);
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleWindow()));

    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(screenGeometryChanged(const QSize&)), this, SLOT(resetShader()));
}

InvertEffect::~InvertEffect()
{
    delete m_shader;
}

bool InvertEffect::supported()
{
    return effects->compositingType() == OpenGL2Compositing;
}

bool InvertEffect::loadData()
{
    m_inited = true;

    QString shadersDir = QStringLiteral("kwin/shaders/1.10/");
#ifdef KWIN_HAVE_OPENGLES
    const qint64 coreVersionNumber = kVersionNumber(3, 0);
#else
    const qint64 coreVersionNumber = kVersionNumber(1, 40);
#endif
    if (GLPlatform::instance()->glslVersion() >= coreVersionNumber)
        shadersDir = QStringLiteral("kwin/shaders/1.40/");
    const QString fragmentshader =  QStandardPaths::locate(QStandardPaths::GenericDataLocation, shadersDir + QStringLiteral("invert.frag"));

    m_shader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, fragmentshader);
    if (!m_shader->isValid()) {
        qCritical() << "The shader failed to load!" << endl;
        return false;
    }

    return true;
}

void InvertEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
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
        ShaderBinder binder(m_shader);
        m_shader->setUniform("screenTransformation", QMatrix4x4());
        m_shader->setUniform("windowTransformation", QMatrix4x4());
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    } else {
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    }
}

void InvertEffect::slotWindowClosed(EffectWindow* w)
{
    m_windows.removeOne(w);
}

void InvertEffect::toggleScreenInversion()
{
    m_allWindows = !m_allWindows;
    effects->addRepaintFull();
}

void InvertEffect::toggleWindow()
{
    if (!effects->activeWindow()) {
        return;
    }
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

bool InvertEffect::provides(Feature f)
{
    return f == ScreenInversion;
}

void InvertEffect::resetShader()
{
    ShaderManager::instance()->resetShader(m_shader, ShaderManager::GenericShader);
}

} // namespace

#include "invert.moc"
