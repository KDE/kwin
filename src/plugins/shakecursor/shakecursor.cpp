/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "plugins/shakecursor/shakecursorconfig.h"
#include "pointer_input.h"
#include "scene/cursoritem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

using namespace std::chrono_literals;

static void ensureResources()
{
    Q_INIT_RESOURCE(shakecursor);
}

namespace KWin
{

BuiltinCursorItem::BuiltinCursorItem(const QImage &image, const QPoint &hotspot, Item *parentItem)
    : Item(parentItem)
{
    m_imageItem = scene()->renderer()->createImageItem(this);
    m_imageItem->setImage(image);
    m_imageItem->setPosition(-hotspot);
    m_imageItem->setSize(image.deviceIndependentSize());
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
        m_scaleAnimation.setDuration(animationTime(200ms));
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
        m_deflateTimer.start(animationTime(2000ms));
    }
}

std::unique_ptr<Item> ShakeCursorEffect::createCursorItem(Item *parentItem) const
{
    // These are regular breeze cursors but with high resolution.
    static const struct
    {
        QString name;
        qreal size;
        QPoint hotspot;
    } builtinCursors[] = {
        {
            .name = QStringLiteral("breeze_cursors"),
            .size = 288,
            .hotspot = QPoint(4, 4),
        },
        {
            .name = QStringLiteral("Breeze_Light"),
            .size = 288,
            .hotspot = QPoint(4, 4),
        },
    };

    for (const auto &builtinCursor : builtinCursors) {
        if (builtinCursor.name == m_cursor->themeName()) {
            QImage image(QStringLiteral(":/effects/shakecursor/%1.png").arg(builtinCursor.name));
            image.setDevicePixelRatio(builtinCursor.size / m_cursor->themeSize());
            if (!image.isNull()) {
                return std::make_unique<BuiltinCursorItem>(image, builtinCursor.hotspot, parentItem);
            }
        }
    }

    return std::make_unique<CursorItem>(effects->scene()->overlayItem());
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

            m_cursorItem = createCursorItem(effects->scene()->overlayItem());
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
