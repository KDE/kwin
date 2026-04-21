/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclickeffect.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "effect/offscreenquickview.h"
#include "input_event.h"
#include "pointer_input.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"
#include "screenitem.h"

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

// Dave, does it make sense to do this? We're always updating the main buffer anyway
// so planes don't help
// it'd make it easier to split this to a new effect.

// then we can re-add the glow

MyCursorItem::MyCursorItem(const CursorTheme &theme, Item *parent)
    : Item(parent)
{
    m_source = std::make_unique<ShapeCursorSource>();
    m_source->setTheme(theme);
    m_source->setShape(Qt::ArrowCursor);

    refresh();
    connect(m_source.get(), &CursorSource::changed, this, &MyCursorItem::refresh);
    connect(m_source.get(), &CursorSource::changed, this, &MyCursorItem::refresh);
}

void MyCursorItem::refresh()
{
    if (!m_imageItem) {
        m_imageItem = std::make_unique<ImageItem>(this);
    }
    m_imageItem->setImage(m_source->image());
    m_imageItem->setPosition(-m_source->hotspot());
    m_imageItem->setSize(m_source->image().deviceIndependentSize());
}

MouseClickEffect2::MouseClickEffect2()
    : m_effectState(new EffectTogglableState(this))
    , m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    auto toggleAction = m_effectState->toggleAction();
    toggleAction->setObjectName(QStringLiteral("MouseClick2"));
    toggleAction->setText(i18nc("@action Mouse Click is the name of a Kwin effect", "Toggle Mouse Click2"));
    toggleAction->setAutoRepeat(false);
    KGlobalAccel::self()->setGlobalShortcut(toggleAction, QKeySequence(Qt::META | Qt::Key_U));

    connect(m_effectState, &EffectTogglableState::statusChanged, this, [this](EffectTogglableState::Status status) {
        if (status == EffectTogglableState::Status::Activating || status == EffectTogglableState::Status::Active) {
            setActive(true);
        }
        if (status == EffectTogglableState::Status::Inactive) {
            setActive(false);
        }
    });

    // ShakeCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
    // setActive(true);
}

MouseClickEffect2::~MouseClickEffect2()
{
}

bool MouseClickEffect2::supported()
{
    return effects->isOpenGLCompositing();
}

QPointF MouseClickEffect2::cursorPos() const
{
    return m_cursor->pos();
}

QPointF MouseClickEffect2::cursorHotSpot() const
{
    return m_cursor->hotspot();
}

bool MouseClickEffect2::isActive() const
{
    return m_active;
}

void MouseClickEffect2::reconfigure(ReconfigureFlags flags)
{
    // ShakeCursorConfig::self()->read();

    //  m_shakeDetector.setInterval(ShakeCursorConfig::timeInterval());
    //  m_shakeDetector.setSensitivity(ShakeCursorConfig::sensitivity());
}

void MouseClickEffect2::initOffscreenViews()
{
    // FIXME not handled screen changes or size changes
    // but hopefully it's a short lived thing
    if (!m_scenesByScreens.empty()) {
        return;
    }
    const auto screens = effects->screens();
    for (const auto output : screens) {
        auto scene = new OffscreenQuickScene();

        qmlRegisterType<ScreenContentsItem>("org.kde.kwin.mouseclick", 1, 0, "ScreenContentsItem");
        scene->loadFromModule(QStringLiteral("org.kde.kwin.mouseclick"), QStringLiteral("Main"), {{QStringLiteral("effect"), QVariant::fromValue(this)}});
        scene->setGeometry(output->geometry());
        m_scenesByScreens[output].reset(scene);
    }
}

void MouseClickEffect2::cleanupOffscreenViews()
{
    m_scenesByScreens.clear();
}

void MouseClickEffect2::pointerMotion(PointerMotionEvent *event)
{
    if (input()->pointer()->isConstrained()) {
        return;
    }
}

void MouseClickEffect2::setActive(bool active)
{
    if (active) {
        initOffscreenViews();

        if (!m_cursorItem) {
            const CursorTheme originalTheme = input()->pointer()->cursorTheme();
            if (m_cursorTheme.name() != originalTheme.name() || m_cursorTheme.size() != originalTheme.size()) {
                m_cursorTheme = CursorTheme(originalTheme.name(), originalTheme.size(), 1);
            }

            m_cursorItem = std::make_unique<MyCursorItem>(m_cursorTheme, effects->scene()->overlayItem());
            m_cursorItem->setPosition(m_cursor->pos());
            connect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), [this]() {
                m_cursorItem->setPosition(m_cursor->pos());
                Q_EMIT cursorPosChanged();
            });
        }
    } else {
        if (m_cursorItem) {
            m_cursorItem.reset();
        }
    }

    m_active = active;
}
/*
void MouseClickEffect2::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &region, KWin::LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);
}*/

} // namespace KWin

#include "moc_mouseclickeffect.cpp"
