/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "startupfeedback.h"
// Qt
#include <QtCore/QSize>
#include <QtGui/QPainter>
// KDE
#include <KDE/KConfigGroup>
#include <KDE/KDebug>
#include <KDE/KGlobal>
#include <KDE/KIconLoader>
#include <KDE/KStandardDirs>
#include <KDE/KStartupInfo>
#include <KDE/KSelectionOwner>
// KWin
#include <kwinglutils.h>
// X11
#include <X11/Xcursor/Xcursor.h>

// based on StartupId in KRunner by Lubos Lunak
// Copyright (C) 2001 Lubos Lunak <l.lunak@kde.org>

namespace KWin
{

KWIN_EFFECT(startupfeedback, StartupFeedbackEffect)
KWIN_EFFECT_SUPPORTED(startupfeedback, StartupFeedbackEffect::supported())

// number of key frames for bouncing animation
static const int BOUNCE_FRAMES = 20;
// duration between two key frames in msec
static const int BOUNCE_FRAME_DURATION = 30;
// duration of one bounce animation
static const int BOUNCE_DURATION = BOUNCE_FRAME_DURATION * BOUNCE_FRAMES;
// number of key frames for blinking animation
static const int BLINKING_FRAMES = 5;
// duration between two key frames in msec
static const int BLINKING_FRAME_DURATION = 100;
// duration of one blinking animation
static const int BLINKING_DURATION = BLINKING_FRAME_DURATION * BLINKING_FRAMES;
//const int color_to_pixmap[] = { 0, 1, 2, 3, 2, 1 };
static const int FRAME_TO_BOUNCE_YOFFSET[] = {
    -5, -1, 2, 5, 8, 10, 12, 13, 15, 15, 15, 15, 14, 12, 10, 8, 5, 2, -1, -5
};
static const QSize BOUNCE_SIZES[] = {
    QSize(16, 16), QSize(14, 18), QSize(12, 20), QSize(18, 14), QSize(20, 12)
};
static const int FRAME_TO_BOUNCE_TEXTURE[] = {
    0, 0, 0, 1, 2, 2, 1, 0, 3, 4, 4, 3, 0, 1, 2, 2, 1, 0, 0, 0
};
static const int FRAME_TO_BLINKING_COLOR[] = {
    0, 1, 2, 3, 2, 1
};
static const QColor BLINKING_COLORS[] = {
    Qt::black, Qt::darkGray, Qt::lightGray, Qt::white, Qt::white
};

StartupFeedbackEffect::StartupFeedbackEffect()
    : m_startupInfo(new KStartupInfo(KStartupInfo::CleanOnCantDetect, this))
    , m_selection(new KSelectionOwner("_KDE_STARTUP_FEEDBACK", -1, this))
    , m_active(false)
    , m_frame(0)
    , m_progress(0)
    , m_texture(0)
    , m_type(BouncingFeedback)
    , m_blinkingShader(0)
{
    for (int i = 0; i < 5; ++i) {
        m_bouncingTextures[i] = 0;
    }
    m_selection->claim(true);
    connect(m_startupInfo, SIGNAL(gotNewStartup(KStartupInfoId,KStartupInfoData)), SLOT(gotNewStartup(KStartupInfoId,KStartupInfoData)));
    connect(m_startupInfo, SIGNAL(gotRemoveStartup(KStartupInfoId,KStartupInfoData)), SLOT(gotRemoveStartup(KStartupInfoId,KStartupInfoData)));
    connect(m_startupInfo, SIGNAL(gotStartupChange(KStartupInfoId,KStartupInfoData)), SLOT(gotStartupChange(KStartupInfoId,KStartupInfoData)));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

StartupFeedbackEffect::~StartupFeedbackEffect()
{
    if (m_active) {
        effects->stopMousePolling();
    }
    for (int i = 0; i < 5; ++i) {
        delete m_bouncingTextures[i];
    }
    delete m_texture;
    delete m_blinkingShader;
}

bool StartupFeedbackEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void StartupFeedbackEffect::reconfigure(Effect::ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    KConfig conf("klaunchrc", KConfig::NoGlobals);
    KConfigGroup c = conf.group("FeedbackStyle");
    const bool busyCursor = c.readEntry("BusyCursor", true);

    c = conf.group("BusyCursorSettings");
    m_startupInfo->setTimeout(c.readEntry("Timeout", 30));
    const bool busyBlinking = c.readEntry("Blinking", false);
    const bool busyBouncing = c.readEntry("Bouncing", true);
    if (!busyCursor)
        m_type = NoFeedback;
    else if (busyBouncing)
        m_type = BouncingFeedback;
    else if (busyBlinking) {
        m_type = BlinkingFeedback;
        if (ShaderManager::instance()->isValid()) {
            delete m_blinkingShader;
            m_blinkingShader = 0;
            const QString shader = KGlobal::dirs()->findResource("data", "kwin/blinking-startup-fragment.glsl");
            m_blinkingShader = ShaderManager::instance()->loadFragmentShader(ShaderManager::SimpleShader, shader);
            if (m_blinkingShader->isValid()) {
                kDebug(1212) << "Blinking Shader is valid";
            } else {
                kDebug(1212) << "Blinking Shader is not valid";
            }
        }
    } else
        m_type = PassiveFeedback;
    if (m_active) {
        stop();
        start(m_startups[ m_currentStartup ]);
    }
}

void StartupFeedbackEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        // need the unclipped version
        switch(m_type) {
        case BouncingFeedback:
            m_progress = (m_progress + time) % BOUNCE_DURATION;
            m_frame = qRound((qreal)m_progress / (qreal)BOUNCE_FRAME_DURATION) % BOUNCE_FRAMES;;
            break;
        case BlinkingFeedback:
            m_progress = (m_progress + time) % BLINKING_DURATION;
            m_frame = qRound((qreal)m_progress / (qreal)BLINKING_FRAME_DURATION) % BLINKING_FRAMES;
            break;
        default:
            break; // nothing
        }
        m_currentGeometry = feedbackRect();
        data.paint.unite(m_currentGeometry);
    }
    effects->prePaintScreen(data, time);
}

void StartupFeedbackEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
    if (m_active) {
        GLTexture* texture;
        switch(m_type) {
        case BouncingFeedback:
            texture = m_bouncingTextures[ FRAME_TO_BOUNCE_TEXTURE[ m_frame ]];
            break;
        case BlinkingFeedback: // fall through
        case PassiveFeedback:
            texture = m_texture;
            break;
        default:
            return; // safety
        }
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
#endif
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        texture->bind();
        bool useShader = false;
        if (m_type == BlinkingFeedback) {
            const QColor& blinkingColor = BLINKING_COLORS[ FRAME_TO_BLINKING_COLOR[ m_frame ]];
            if (m_blinkingShader && m_blinkingShader->isValid()) {
                useShader = true;
                ShaderManager::instance()->pushShader(m_blinkingShader);
                m_blinkingShader->setUniform("u_color", blinkingColor);
            } else {
#ifndef KWIN_HAVE_OPENGLES
                // texture transformation
                float color[4] = { blinkingColor.redF(), blinkingColor.greenF(), blinkingColor.blueF(), 1.0f };
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
                glColor4fv(color);
                glActiveTexture(GL_TEXTURE1);
                texture->bind();
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
                glActiveTexture(GL_TEXTURE0);
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
#endif
            }
        } else if (ShaderManager::instance()->isValid()) {
            useShader = true;
            ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
        }
        texture->render(m_currentGeometry, m_currentGeometry);
        if (useShader) {
            ShaderManager::instance()->popShader();
        }
        if (m_type == BlinkingFeedback && !useShader) {
#ifndef KWIN_HAVE_OPENGLES
            // resture states
            glActiveTexture(GL_TEXTURE1);
            texture->unbind();
            glActiveTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
#endif
        }
        texture->unbind();
        glDisable(GL_BLEND);
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }
}

void StartupFeedbackEffect::postPaintScreen()
{
    if (m_active) {
        switch(m_type) {
        case BouncingFeedback: // fall through
        case BlinkingFeedback:
            // repaint the icon
            effects->addRepaint(m_currentGeometry);
            break;
        case PassiveFeedback: // fall through
        default:
            // no need to repaint - no change
            break;
        }
    }
    effects->postPaintScreen();
}

void StartupFeedbackEffect::slotMouseChanged(const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons,
        Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers)
{
    Q_UNUSED(pos)
    Q_UNUSED(oldpos)
    Q_UNUSED(buttons)
    Q_UNUSED(oldbuttons)
    Q_UNUSED(modifiers)
    Q_UNUSED(oldmodifiers)
    if (m_active) {
        effects->addRepaint(m_currentGeometry);
        effects->addRepaint(feedbackRect());
    }
}

void StartupFeedbackEffect::gotNewStartup(const KStartupInfoId& id, const KStartupInfoData& data)
{
    const QString& icon = data.findIcon();
    m_currentStartup = id;
    m_startups[ id ] = icon;
    start(icon);
}

void StartupFeedbackEffect::gotRemoveStartup(const KStartupInfoId& id, const KStartupInfoData& data)
{
    Q_UNUSED( data )
    m_startups.remove(id);
    if (m_startups.count() == 0) {
        m_currentStartup = KStartupInfoId(); // null
        stop();
        return;
    }
    m_currentStartup = m_startups.begin().key();
    start(m_startups[ m_currentStartup ]);
}

void StartupFeedbackEffect::gotStartupChange(const KStartupInfoId& id, const KStartupInfoData& data)
{
    if (m_currentStartup == id) {
        const QString& icon = data.findIcon();
        if (!icon.isEmpty() && icon != m_startups[ m_currentStartup ]) {
            m_startups[ id ] = icon;
            start(icon);
        }
    }
}

void StartupFeedbackEffect::start(const QString& icon)
{
    if (m_type == NoFeedback)
        return;
    if (!m_active)
        effects->startMousePolling();
    m_active = true;
    QPixmap iconPixmap = KIconLoader::global()->loadIcon(icon, KIconLoader::Small, 0,
                         KIconLoader::DefaultState, QStringList(), 0, true);  // return null pixmap if not found
    if (iconPixmap.isNull())
        iconPixmap = SmallIcon("system-run");
    prepareTextures(iconPixmap);
    effects->addRepaintFull();
}

void StartupFeedbackEffect::stop()
{
    if (m_active)
        effects->stopMousePolling();
    m_active = false;
    switch(m_type) {
    case BouncingFeedback:
        for (int i = 0; i < 5; ++i) {
            delete m_bouncingTextures[i];
            m_bouncingTextures[i] = 0;
        }
        break;
    case BlinkingFeedback:
    case PassiveFeedback:
        delete m_texture;
        m_texture = 0;
        break;
    case NoFeedback:
        return; // don't want the full repaint
    default:
        break; // impossible
    }
    effects->addRepaintFull();
}

void StartupFeedbackEffect::prepareTextures(const QPixmap& pix)
{
    switch(m_type) {
    case BouncingFeedback:
        for (int i = 0; i < 5; ++i) {
            delete m_bouncingTextures[i];
            m_bouncingTextures[i] = new GLTexture(scalePixmap(pix, BOUNCE_SIZES[i]));
        }
        break;
    case BlinkingFeedback:
    case PassiveFeedback:
        m_texture = new GLTexture(pix);
        break;
    default:
        // for safety
        m_active = false;
        break;
    }
}

QImage StartupFeedbackEffect::scalePixmap(const QPixmap& pm, const QSize& size) const
{
    QImage scaled = pm.toImage().scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (scaled.format() != QImage::Format_ARGB32_Premultiplied && scaled.format() != QImage::Format_ARGB32)
        scaled = scaled.convertToFormat(QImage::Format_ARGB32);

    QImage result(20, 20, QImage::Format_ARGB32);
    QPainter p(&result);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(result.rect(), Qt::transparent);
    p.drawImage((20 - size.width()) / 2, (20 - size.height()) / 2, scaled, 0, 0, size.width(), size.height());
    return result;
}

QRect StartupFeedbackEffect::feedbackRect() const
{
    int cursorSize = 0;
    cursorSize = XcursorGetDefaultSize(QX11Info::display());
    int xDiff;
    if (cursorSize <= 16)
        xDiff = 8 + 7;
    else if (cursorSize <= 32)
        xDiff = 16 + 7;
    else if (cursorSize <= 48)
        xDiff = 24 + 7;
    else
        xDiff = 32 + 7;
    int yDiff = xDiff;
    GLTexture* texture = 0;
    int yOffset = 0;
    switch(m_type) {
    case BouncingFeedback:
        texture = m_bouncingTextures[ FRAME_TO_BOUNCE_TEXTURE[ m_frame ]];
        yOffset = FRAME_TO_BOUNCE_YOFFSET[ m_frame ];
        break;
    case BlinkingFeedback: // fall through
    case PassiveFeedback:
        texture = m_texture;
        break;
    default:
        // nothing
        break;
    }
    const QPoint cursorPos = effects->cursorPos() + QPoint(xDiff, yDiff + yOffset);
    QRect rect;
    if( texture )
       rect = QRect(cursorPos, texture->size());
    return rect;
}

bool StartupFeedbackEffect::isActive() const
{
    return m_active;
}

} // namespace
