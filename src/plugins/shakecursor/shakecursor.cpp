/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "plugins/shakecursor/shakecursorconfig.h"
#include "pointer_input.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

static void ensureResources()
{
    Q_INIT_RESOURCE(shakecursor);
}

namespace KWin
{

ShakeCursorItem::ShakeCursorItem(const KXcursorTheme &theme, Item *parent)
    : Item(parent)
{
    m_source = std::make_unique<ShapeCursorSource>();
    m_source->setTheme(theme);
    m_source->setShape(Qt::ArrowCursor);

    refresh();
    connect(m_source.get(), &CursorSource::changed, this, &ShakeCursorItem::refresh);
}

void ShakeCursorItem::refresh()
{
    if (!m_imageItem) {
        m_imageItem = scene()->renderer()->createImageItem(this);
    }
    m_imageItem->setImage(m_source->image());
    m_imageItem->setPosition(-m_source->hotspot());
    m_imageItem->setSize(m_source->image().deviceIndependentSize());
}

ShakeCursorEffect::ShakeCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    ensureResources();
    input()->installInputEventSpy(this);

    m_deflateTimer.setSingleShot(true);
    connect(&m_deflateTimer, &QTimer::timeout, this, &ShakeCursorEffect::deflate);

    connect(&m_scaleAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        magnify(m_scaleAnimation.currentValue().toReal());
    });

    ShakeCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

ShakeCursorEffect::~ShakeCursorEffect()
{
    magnify(1.0);
}

bool ShakeCursorEffect::supported()
{
    if (!effects->waylandDisplay()) {
        return false;
    }
    return effects->isOpenGLCompositing();
}

bool ShakeCursorEffect::isActive() const
{
    return m_currentMagnification != 1.0;
}

void ShakeCursorEffect::reconfigure(ReconfigureFlags flags)
{
    ShakeCursorConfig::self()->read();

    m_shakeDetector.setInterval(ShakeCursorConfig::timeInterval());
    m_shakeDetector.setSensitivity(ShakeCursorConfig::sensitivity());
}

void ShakeCursorEffect::inflate()
{
    qreal magnification;
    if (m_targetMagnification == 1.0) {
        magnification = ShakeCursorConfig::magnification();
    } else {
        magnification = m_targetMagnification + ShakeCursorConfig::overMagnification();
    }

    animateTo(magnification);
}

void ShakeCursorEffect::deflate()
{
    animateTo(1.0);
}

void ShakeCursorEffect::animateTo(qreal magnification)
{
    if (m_targetMagnification != magnification) {
        m_scaleAnimation.stop();

        m_scaleAnimation.setStartValue(m_currentMagnification);
        m_scaleAnimation.setEndValue(magnification);
        m_scaleAnimation.setDuration(200); // ignore animation speed, it's not an animation from user perspective
        m_scaleAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        m_scaleAnimation.start();

        m_targetMagnification = magnification;
    }
}

void ShakeCursorEffect::pointerEvent(MouseEvent *event)
{
    if (event->buttons() != Qt::NoButton) {
        m_shakeDetector.reset();
        return;
    }

    if (input()->pointer()->isConstrained() || event->type() != QEvent::MouseMove) {
        return;
    }

    if (m_shakeDetector.update(event)) {
        inflate();
        m_deflateTimer.start(2000);
    }
}

void ShakeCursorEffect::magnify(qreal magnification)
{
    if (magnification == 1.0) {
        m_currentMagnification = 1.0;
        if (m_cursorItem) {
            m_cursorItem.reset();
            effects->showCursor();
        }
    } else {
        m_currentMagnification = magnification;

        if (!m_cursorItem) {
            effects->hideCursor();

            const qreal maxScale = ShakeCursorConfig::magnification() + 4 * ShakeCursorConfig::overMagnification();
            if (m_cursorTheme.name() != m_cursor->themeName() || m_cursorTheme.size() != m_cursor->themeSize() || m_cursorTheme.devicePixelRatio() != maxScale) {
                static const QStringList embeddedCursorThemes{
                    QStringLiteral("breeze_cursors"),
                    QStringLiteral("Breeze_Light"),
                };

                QStringList searchPaths;
                if (embeddedCursorThemes.contains(m_cursor->themeName())) {
                    searchPaths.append(QStringLiteral(":/effects/shakecursor/cursors"));
                }
                m_cursorTheme = KXcursorTheme(m_cursor->themeName(), m_cursor->themeSize(), maxScale, searchPaths);
            }

            m_cursorItem = std::make_unique<ShakeCursorItem>(m_cursorTheme, effects->scene()->overlayItem());
            m_cursorItem->setPosition(m_cursor->pos());
            connect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), [this]() {
                m_cursorItem->setPosition(m_cursor->pos());
            });
        }
        m_cursorItem->setTransform(QTransform::fromScale(magnification, magnification));
    }
}

} // namespace KWin

#include "moc_shakecursor.cpp"
