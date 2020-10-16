/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "startupfeedback.h"
// Qt
#include <QApplication>
#include <QFile>
#include <QSize>
#include <QStyle>
#include <QStandardPaths>
#include <QPainter>
// KDE
#include <KConfigGroup>
#include <KSharedConfig>
#include <KStartupInfo>
#include <KSelectionOwner>
#include <KWindowSystem>
// KWin
#include <kwinglutils.h>

// based on StartupId in KRunner by Lubos Lunak
// SPDX-FileCopyrightText: 2001 Lubos Lunak <l.lunak@kde.org>

namespace KWin
{

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
static const int s_startupDefaultTimeout = 5;

StartupFeedbackEffect::StartupFeedbackEffect()
    : m_bounceSizesRatio(1.0)
    , m_startupInfo(new KStartupInfo(KStartupInfo::CleanOnCantDetect, this))
    , m_selection(nullptr)
    , m_active(false)
    , m_frame(0)
    , m_progress(0)
    , m_texture(nullptr)
    , m_type(BouncingFeedback)
    , m_blinkingShader(nullptr)
    , m_cursorSize(24)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("klaunchrc", KConfig::NoGlobals)))
{
    for (int i = 0; i < 5; ++i) {
        m_bouncingTextures[i] = nullptr;
    }
    if (KWindowSystem::isPlatformX11()) {
        m_selection = new KSelectionOwner("_KDE_STARTUP_FEEDBACK", xcbConnection(), x11RootWindow(), this);
        m_selection->claim(true);
    }
    connect(m_startupInfo, &KStartupInfo::gotNewStartup, this, &StartupFeedbackEffect::gotNewStartup);
    connect(m_startupInfo, &KStartupInfo::gotRemoveStartup, this, &StartupFeedbackEffect::gotRemoveStartup);
    connect(m_startupInfo, &KStartupInfo::gotStartupChange, this, &StartupFeedbackEffect::gotStartupChange);
    connect(effects, &EffectsHandler::mouseChanged, this, &StartupFeedbackEffect::slotMouseChanged);
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this]() {
        reconfigure(ReconfigureAll);
    });
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
    return effects->isOpenGLCompositing();
}

void StartupFeedbackEffect::reconfigure(Effect::ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    KConfigGroup c = m_configWatcher->config()->group("FeedbackStyle");
    const bool busyCursor = c.readEntry("BusyCursor", true);

    c = m_configWatcher->config()->group("BusyCursorSettings");
    m_startupInfo->setTimeout(c.readEntry("Timeout", s_startupDefaultTimeout));
    const bool busyBlinking = c.readEntry("Blinking", false);
    const bool busyBouncing = c.readEntry("Bouncing", true);
    if (!busyCursor)
        m_type = NoFeedback;
    else if (busyBouncing)
        m_type = BouncingFeedback;
    else if (busyBlinking) {
        m_type = BlinkingFeedback;
        if (effects->compositingType() == OpenGL2Compositing) {
            delete m_blinkingShader;
            m_blinkingShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("blinking-startup-fragment.glsl"));
            if (m_blinkingShader->isValid()) {
                qCDebug(KWINEFFECTS) << "Blinking Shader is valid";
            } else {
                qCDebug(KWINEFFECTS) << "Blinking Shader is not valid";
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
            m_frame = qRound((qreal)m_progress / (qreal)BOUNCE_FRAME_DURATION) % BOUNCE_FRAMES;
            m_currentGeometry = feedbackRect(); // bounce alters geometry with m_frame
            data.paint = data.paint.united(m_currentGeometry);
            break;
        case BlinkingFeedback:
            m_progress = (m_progress + time) % BLINKING_DURATION;
            m_frame = qRound((qreal)m_progress / (qreal)BLINKING_FRAME_DURATION) % BLINKING_FRAMES;
            break;
        default:
            break; // nothing
        }
    }
    effects->prePaintScreen(data, time);
}

void StartupFeedbackEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
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
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        texture->bind();
        if (m_type == BlinkingFeedback && m_blinkingShader && m_blinkingShader->isValid()) {
            const QColor& blinkingColor = BLINKING_COLORS[ FRAME_TO_BLINKING_COLOR[ m_frame ]];
            ShaderManager::instance()->pushShader(m_blinkingShader);
            m_blinkingShader->setUniform(GLShader::Color, blinkingColor);
        } else {
            ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
        }
        QMatrix4x4 mvp = data.projectionMatrix();
        mvp.translate(m_currentGeometry.x(), m_currentGeometry.y());
        ShaderManager::instance()->getBoundShader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        texture->render(m_currentGeometry, m_currentGeometry);
        ShaderManager::instance()->popShader();
        texture->unbind();
        glDisable(GL_BLEND);
    }
}

void StartupFeedbackEffect::postPaintScreen()
{
    if (m_active) {
        m_dirtyRect = m_currentGeometry; // ensure the now dirty region is cleaned on the next pass
        if (m_type == BlinkingFeedback || m_type == BouncingFeedback)
            effects->addRepaint(m_dirtyRect); // we also have to trigger a repaint
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
        m_dirtyRect |= m_currentGeometry;
        m_currentGeometry = feedbackRect();
        m_dirtyRect |= m_currentGeometry;
        effects->addRepaint(m_dirtyRect);
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
    auto readCursorSize = []() -> int {
        // read details about the mouse-cursor theme define per default
        KConfigGroup mousecfg(effects->inputConfig(), "Mouse");
        int cursorSize  = mousecfg.readEntry("cursorSize", 24);
        return cursorSize;
    };
    m_cursorSize = readCursorSize();
    int iconSize = m_cursorSize / 1.5;
    if (!iconSize) {
        iconSize = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    }
    // get ratio for bouncing cursor so we don't need to manually calculate the sizes for each icon size
    if (m_type == BouncingFeedback)
        m_bounceSizesRatio = iconSize / 16.0;
    const QPixmap iconPixmap = QIcon::fromTheme(icon, QIcon::fromTheme(QStringLiteral("system-run"))).pixmap(iconSize);
    prepareTextures(iconPixmap);
    m_dirtyRect = m_currentGeometry = feedbackRect();
    effects->addRepaint(m_dirtyRect);
}

void StartupFeedbackEffect::stop()
{
    if (m_active)
        effects->stopMousePolling();
    m_active = false;
    effects->makeOpenGLContextCurrent();
    switch(m_type) {
    case BouncingFeedback:
        for (int i = 0; i < 5; ++i) {
            delete m_bouncingTextures[i];
            m_bouncingTextures[i] = nullptr;
        }
        break;
    case BlinkingFeedback:
    case PassiveFeedback:
        delete m_texture;
        m_texture = nullptr;
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
    effects->makeOpenGLContextCurrent();
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
    const QSize& adjustedSize = size * m_bounceSizesRatio;
    QImage scaled = pm.toImage().scaled(adjustedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (scaled.format() != QImage::Format_ARGB32_Premultiplied && scaled.format() != QImage::Format_ARGB32)
        scaled = scaled.convertToFormat(QImage::Format_ARGB32);

    QImage result(20 * m_bounceSizesRatio, 20 * m_bounceSizesRatio, QImage::Format_ARGB32);
    QPainter p(&result);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(result.rect(), Qt::transparent);
    p.drawImage((20 * m_bounceSizesRatio - adjustedSize.width()) / 2, (20*m_bounceSizesRatio - adjustedSize.height()) / 2, scaled, 0, 0, adjustedSize.width(), adjustedSize.height() * m_bounceSizesRatio);
    return result;
}

QRect StartupFeedbackEffect::feedbackRect() const
{
    int xDiff;
    if (m_cursorSize <= 16)
        xDiff = 8 + 7;
    else if (m_cursorSize <= 32)
        xDiff = 16 + 7;
    else if (m_cursorSize <= 48)
        xDiff = 24 + 7;
    else
        xDiff = 32 + 7;
    int yDiff = xDiff;
    GLTexture* texture = nullptr;
    int yOffset = 0;
    switch(m_type) {
    case BouncingFeedback:
        texture = m_bouncingTextures[ FRAME_TO_BOUNCE_TEXTURE[ m_frame ]];
        yOffset = FRAME_TO_BOUNCE_YOFFSET[ m_frame ] * m_bounceSizesRatio;
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
