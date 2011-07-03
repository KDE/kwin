/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#include "boxswitch.h"

#include <kwinconfig.h>

#include <QMouseEvent>
#include <QSize>

#include <kapplication.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT(boxswitch, BoxSwitchEffect)

BoxSwitchEffect::BoxSwitchEffect()
    : mActivated(0)
    , mMode(0)
    , thumbnailFrame(effects->effectFrame(EffectFrameStyled))
    , selected_window(0)
    , painting_desktop(0)
    , animation(false)
    , highlight_is_set(false)
    , primaryTabBox(true)
    , secondaryTabBox(false)
    , mProxy(this)
    , mProxyActivated(0)
    , mProxyAnimateSwitch(0)
    , mProxyShowText(0)
    , mPositioningFactor(0.5f)
{
    text_font.setBold(true);
    text_font.setPointSize(12);
    thumbnailFrame->setFont(text_font);
    thumbnailFrame->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

    highlight_margin = 10;
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowClosed(EffectWindow*)), this, SLOT(slotWindowClosed(EffectWindow*)));
    connect(effects, SIGNAL(tabBoxAdded(int)), this, SLOT(slotTabBoxAdded(int)));
    connect(effects, SIGNAL(tabBoxClosed()), this, SLOT(slotTabBoxClosed()));
    connect(effects, SIGNAL(tabBoxUpdated()), this, SLOT(slotTabBoxUpdated()));
    connect(effects, SIGNAL(windowGeometryShapeChanged(EffectWindow*,QRect)), this, SLOT(slotWindowGeometryShapeChanged(EffectWindow*,QRect)));
    connect(effects, SIGNAL(windowDamaged(EffectWindow*,QRect)), this, SLOT(slotWindowDamaged(EffectWindow*,QRect)));
}

BoxSwitchEffect::~BoxSwitchEffect()
{
    delete thumbnailFrame;
}

void BoxSwitchEffect::reconfigure(ReconfigureFlags)
{
    color_frame = KColorScheme(QPalette::Active, KColorScheme::Window).background().color();
    color_frame.setAlphaF(0.9);
    color_highlight = KColorScheme(QPalette::Active, KColorScheme::Selection).background().color();
    color_highlight.setAlphaF(0.9);
    activeTimeLine.setDuration(animationTime(250));
    activeTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    timeLine.setDuration(animationTime(150));
    timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    KConfigGroup conf = effects->effectConfig("BoxSwitch");

    bg_opacity = conf.readEntry("BackgroundOpacity", 25) / 100.0;
    elevate_window = conf.readEntry("ElevateSelected", true);
    mAnimateSwitch = conf.readEntry("AnimateSwitch", false);

    primaryTabBox = conf.readEntry("TabBox", true);
    secondaryTabBox = conf.readEntry("TabBoxAlternative", false);
}

void BoxSwitchEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (activeTimeLine.currentValue() != 0.0 && !mProxyActivated) {
        if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
            if (windows.contains(w)) {
                if (w == selected_window)
                    w->enablePainting(EffectWindow::PAINT_DISABLED_BY_CLIENT_GROUP);
                else
                    data.setTranslucent();
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            }
        } else {
            if (painting_desktop) {
                if (w->isOnDesktop(painting_desktop))
                    w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
                else
                    w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            }
        }
    }
    effects->prePaintWindow(w, data, time);
}

void BoxSwitchEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (mActivated)
        activeTimeLine.setCurrentTime(activeTimeLine.currentTime() + time);
    else {
        activeTimeLine.setCurrentTime(activeTimeLine.currentTime() - time);
        if (activeTimeLine.currentValue() == 0.0) {
            // No longer need the window data
            qDeleteAll(windows);
            windows.clear();
        }
    }
    if (mActivated && animation) {
        timeLine.setCurrentTime(timeLine.currentTime() + time);
        calculateItemSizes();
    }
    effects->prePaintScreen(data, time);
}

void BoxSwitchEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
    if (mActivated && !mProxyActivated) {
        if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
            paintWindowsBox(region);
        } else {
            if (!painting_desktop) {
                thumbnailFrame->setSelection(desktops[ selected_desktop ]->area);
                thumbnailFrame->render(region);

                QHash< int, ItemInfo* >::const_iterator i;
                for (i = desktops.constBegin(); i != desktops.constEnd(); ++i) {
                    painting_desktop = i.key();
                    paintDesktopThumbnail(painting_desktop);
                }
                painting_desktop = 0;
            }
        }
    }
}

void BoxSwitchEffect::paintWindowsBox(const QRegion& region)
{
    if ((mAnimateSwitch && !mProxyActivated) || (mProxyActivated && mProxyAnimateSwitch))
        thumbnailFrame->setSelection(highlight_area);
    else {
        ItemInfo *info = windows.value(selected_window, 0);
        thumbnailFrame->setSelection(info ? info->area : QRect());
    }
    thumbnailFrame->render(region);

    if ((mAnimateSwitch && !mProxyActivated) || (mProxyActivated && mProxyAnimateSwitch)) {
        // HACK: PaintClipper is used because window split is somehow wrong if the height is greater than width
        PaintClipper::push(frame_area);
        QHash< EffectWindow*, ItemInfo* >::const_iterator i;
        for (i = windows.constBegin(); i != windows.constEnd(); ++i) {
            paintWindowThumbnail(i.key());
            paintWindowIcon(i.key());
        }
        PaintClipper::pop(frame_area);
    } else {
        QHash< EffectWindow*, ItemInfo* >::const_iterator i;
        for (i = windows.constBegin(); i != windows.constEnd(); ++i) {
            paintWindowThumbnail(i.key());
            paintWindowIcon(i.key());
        }
    }
}

void BoxSwitchEffect::postPaintScreen()
{
    if (mActivated && activeTimeLine.currentValue() != 1.0)
        effects->addRepaintFull();
    if (!mActivated && activeTimeLine.currentValue() != 0.0)
        effects->addRepaintFull();
    if (mActivated && animation) {
        if (timeLine.currentValue() == 1.0) {
            timeLine.setCurrentTime(0);
            animation = false;
            if (!scheduled_directions.isEmpty()) {
                direction = scheduled_directions.dequeue();
                animation = true;
            }
        }
        QRect repaint = QRect(frame_area.x() - item_max_size.width() * 0.5,
                              frame_area.y(),
                              frame_area.width() + item_max_size.width(),
                              frame_area.height());
        effects->addRepaint(repaint);
    }
    effects->postPaintScreen();
}

void BoxSwitchEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (((mActivated && (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode))
            || (!mActivated && activeTimeLine.currentValue() != 0.0)) && !mProxyActivated) {
        if (windows.contains(w) && w != selected_window) {
            if (w->isMinimized() || !w->isOnCurrentDesktop())
                // TODO: When deactivating minimized windows are not painted at all
                data.opacity *= activeTimeLine.currentValue() * bg_opacity;
            else
                data.opacity *= 1.0 - activeTimeLine.currentValue() * (1.0 - bg_opacity);
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void BoxSwitchEffect::windowInputMouseEvent(Window w, QEvent* e)
{
    assert(w == mInput);
    Q_UNUSED(w);
    if (e->type() != QEvent::MouseButtonPress)
        return;
    QPoint pos = static_cast< QMouseEvent* >(e)->pos();
    pos += frame_area.topLeft();

    // determine which item was clicked
    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        QHash< EffectWindow*, ItemInfo* >::const_iterator i;
        for (i = windows.constBegin(); i != windows.constEnd(); ++i) {
            if (i.value()->clickable.contains(pos)) {
                effects->setTabBoxWindow(i.key());
                break;
            }
        }
        // special handling for second half of window in case of animation and even number of windows
        if (mAnimateSwitch && (windows.size() % 2 == 0)) {
            QRect additionalRect = QRect(frame_area.x(), frame_area.y(),
                                         item_max_size.width() * 0.5, item_max_size.height());
            if (additionalRect.contains(pos)) {
                effects->setTabBoxWindow(right_window);
            }
        }
    } else {
        QHash< int, ItemInfo* >::const_iterator i;
        for (i = desktops.constBegin(); i != desktops.constEnd(); ++i) {
            if (i.value()->clickable.contains(pos))
                effects->setTabBoxDesktop(i.key());
        }
    }
}

void BoxSwitchEffect::slotWindowDamaged(EffectWindow* w, const QRect& damage)
{
    Q_UNUSED(damage);
    if (mActivated) {
        if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
            if (windows.contains(w)) {
                effects->addRepaint(frame_area);
            }
        } else {
            if (w->isOnAllDesktops()) {
                foreach (ItemInfo * info, desktops)
                effects->addRepaint(info->area);
            } else {
                effects->addRepaint(desktops[ w->desktop()]->area);
            }
        }
#ifdef KWIN_HAVE_OPENGLES
        // without full repaints, blur effect flickers on GLES, see BUG 272688
        effects->addRepaintFull();
#endif
    }
}

void BoxSwitchEffect::slotWindowGeometryShapeChanged(EffectWindow* w, const QRect& old)
{
    if (mActivated) {
        if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
            ItemInfo *info = windows.value( w, 0L );
            if (info && w->size() != old.size()) {
                effects->addRepaint(info->area);
            }
        } else {
            if (w->isOnAllDesktops()) {
                foreach (ItemInfo * info, desktops)
                effects->addRepaint(info->area);
            } else {
                effects->addRepaint(desktops[ w->desktop()]->area);
            }
        }
    }
}

void BoxSwitchEffect::slotTabBoxAdded(int mode)
{
    if (!mActivated) {
        if ((mode == TabBoxWindowsMode && primaryTabBox) ||
                (mode == TabBoxWindowsAlternativeMode && secondaryTabBox)) {
            if (effects->currentTabBoxWindowList().count() > 0) {
                mMode = mode;
                effects->refTabBox();
                highlight_is_set = false;
                animation = false;
                scheduled_directions.clear();
                right_window = 0;
                setActive();
            }
        } else if (mode == TabBoxDesktopListMode || mode == TabBoxDesktopMode) {
            // DesktopMode
            if (effects->currentTabBoxDesktopList().count() > 0) {
                mMode = mode;
                painting_desktop = 0;
                effects->refTabBox();
                setActive();
            }
        }
    }
}

void BoxSwitchEffect::slotTabBoxClosed()
{
    if (mActivated)
        setInactive();
}

void BoxSwitchEffect::slotTabBoxUpdated()
{
    if (mActivated) {
        if ((mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode)
                && selected_window != effects->currentTabBoxWindow()) {
            if (selected_window != NULL) {
                if ((mAnimateSwitch && !mProxyActivated) || (mProxyActivated && mProxyAnimateSwitch)) {
                    int old_index = effects->currentTabBoxWindowList().indexOf(selected_window);
                    int new_index = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                    Direction new_direction;
                    int distance = new_index - old_index;
                    if (distance > 0)
                        new_direction = Left;
                    if (distance < 0)
                        new_direction = Right;
                    if (distance != 0) {
                        distance = abs(distance);
                        int tempDistance = effects->currentTabBoxWindowList().count() - distance;
                        if (tempDistance < abs(distance)) {
                            distance = tempDistance;
                            if (new_direction == Left)
                                new_direction = Right;
                            else
                                new_direction = Left;
                        }
                        if (!animation) {
                            animation = true;
                            direction = new_direction;
                            distance--;
                        }
                        for (int i = 0; i < distance; i++) {
                            if (!scheduled_directions.isEmpty() && scheduled_directions.last() != new_direction)
                                scheduled_directions.pop_back();
                            else
                                scheduled_directions.enqueue(new_direction);
                            if (scheduled_directions.count() == effects->currentTabBoxWindowList().count())
                                scheduled_directions.clear();
                        }
                    }
                }
                if (ItemInfo *info = windows.value(selected_window, 0))
                    effects->addRepaint(info->area);
                selected_window->addRepaintFull();
            }
            setSelectedWindow(effects->currentTabBoxWindow());
            if (ItemInfo *info = windows.value(selected_window, 0))
                effects->addRepaint(info->area);
            if (selected_window) // @Martin can effects->currentTabBoxWindow() be NULL?
                selected_window->addRepaintFull();
            effects->addRepaint(text_area);
        } else if (mMode != TabBoxWindowsMode && mMode != TabBoxWindowsAlternativeMode) {
            // DesktopMode
            if (ItemInfo *info = desktops.value(selected_desktop, 0))
                effects->addRepaint(info->area);
            selected_desktop = effects->currentTabBoxDesktop();
            if (!mProxyActivated || mProxyShowText)
                thumbnailFrame->setText(effects->desktopName(selected_desktop));
            if (ItemInfo *info = desktops.value(selected_desktop, 0))
                effects->addRepaint(info->area);
            effects->addRepaint(text_area);
            if (effects->currentTabBoxDesktopList() == original_desktops)
                return;
            original_desktops = effects->currentTabBoxDesktopList();
        }
        // for safety copy list each time tabbox is updated
        original_windows = effects->currentTabBoxWindowList();
        effects->addRepaint(frame_area);
        calculateFrameSize();
        calculateItemSizes();
        moveResizeInputWindow(frame_area.x(), frame_area.y(), frame_area.width(), frame_area.height());
        effects->addRepaint(frame_area);
    }
}

void BoxSwitchEffect::setActive()
{
    mActivated = true;

    // Just in case we are activated again before the deactivation animation finished
    qDeleteAll(windows);
    windows.clear();

    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        original_windows = effects->currentTabBoxWindowList();
        setSelectedWindow(effects->currentTabBoxWindow());
    } else {
        original_desktops = effects->currentTabBoxDesktopList();
        selected_desktop = effects->currentTabBoxDesktop();
        if (!mProxyActivated || mProxyShowText)
            thumbnailFrame->setText(effects->desktopName(selected_desktop));
    }
    calculateFrameSize();
    calculateItemSizes();
    if (!mProxyActivated) {
        // only create the input window when effect is not activated via the proxy
        mInput = effects->createInputWindow(this, frame_area.x(), frame_area.y(),
                                            frame_area.width(), frame_area.height(), Qt::ArrowCursor);
    }
    effects->addRepaint(frame_area);
    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        QHash< EffectWindow*, ItemInfo* >::const_iterator i;
        for (i = windows.constBegin(); i != windows.constEnd(); ++i) {
            i.key()->addRepaintFull();
        }
    }
}

void BoxSwitchEffect::setInactive()
{
    mActivated = false;
    effects->unrefTabBox();
    if (!mProxyActivated && mInput != None) {
        effects->destroyInputWindow(mInput);
        mInput = None;
    }
    mProxyActivated = false;
    mPositioningFactor = 0.5f;
    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        QHash< EffectWindow*, ItemInfo* >::const_iterator i;
        for (i = windows.constBegin(); i != windows.constEnd(); ++i) {
            if (i.key() != selected_window)
                i.key()->addRepaintFull();
        }
        // We don't unset the selected window so we have correct fading
        // But we do need to remove elevation status
        if (elevate_window && selected_window)
            effects->setElevatedWindow(selected_window, false);

        foreach (EffectWindow * w, referrencedWindows) {
            w->unrefWindow();
        }
        referrencedWindows.clear();
    } else {
        // DesktopMode
        qDeleteAll(windows);
        desktops.clear();
    }
    thumbnailFrame->free();
    effects->addRepaint(frame_area);
    frame_area = QRect();
}

void BoxSwitchEffect::setSelectedWindow(EffectWindow* w)
{
    if (elevate_window && selected_window) {
        effects->setElevatedWindow(selected_window, false);
    }
    selected_window = w;
    if (selected_window && (!mProxyActivated || mProxyShowText))
        thumbnailFrame->setText(selected_window->caption());
    if (elevate_window && w) {
        effects->setElevatedWindow(selected_window, true);
    }
}

void BoxSwitchEffect::slotWindowClosed(EffectWindow* w)
{
    if (w == selected_window) {
        setSelectedWindow(0);
    }
    QHash<EffectWindow*, ItemInfo*>::iterator it = windows.find(w);
    if (it != windows.end()) {
        w->refWindow();
        referrencedWindows.append(w);
        original_windows.removeAll(w);
        delete *it; *it = 0;
        windows.erase( it );
        effects->addRepaintFull();
    }
}

void BoxSwitchEffect::moveResizeInputWindow(int x, int y, int width, int height)
{
    XMoveWindow(display(), mInput, x, y);
    XResizeWindow(display(), mInput, width, height);
}

void BoxSwitchEffect::calculateFrameSize()
{
    int itemcount;

    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        itemcount = original_windows.count();
        item_max_size.setWidth(200);
        item_max_size.setHeight(200);
    } else {
        itemcount = original_desktops.count();
        item_max_size.setWidth(200);
        item_max_size.setHeight(200);
    }
    // How much height to reserve for a one-line text label
    text_area.setHeight(QFontMetrics(text_font).height() * 1.2);
    // Separator space between items and text
    const int separator_height = 6;
    // Shrink the size until all windows/desktops can fit onscreen
    frame_area.setWidth(itemcount * item_max_size.width());
    QRect screenr = effects->clientArea(PlacementArea, effects->activeScreen(), effects->currentDesktop());
    while (frame_area.width() > screenr.width()) {
        item_max_size /= 2;
        frame_area.setWidth(itemcount * item_max_size.width());
    }
    frame_area.setHeight(item_max_size.height() +
                         separator_height + text_area.height());
    if (mProxyActivated && !mProxyShowText)
        frame_area.setHeight(item_max_size.height());
    text_area.setWidth(frame_area.width());

    frame_area.moveTo(screenr.x() + (screenr.width() - frame_area.width()) / 2,
                      screenr.y() + (screenr.height() - frame_area.height()) / 2 * mPositioningFactor * 2);
    text_area.moveTo(frame_area.x(),
                     frame_area.y() + item_max_size.height() + separator_height);

    thumbnailFrame->setGeometry(frame_area);
}

void BoxSwitchEffect::calculateItemSizes()
{
    if (mMode == TabBoxWindowsMode || mMode == TabBoxWindowsAlternativeMode) {
        qDeleteAll(windows);
        windows.clear();
        if ((mAnimateSwitch && !mProxyActivated) || (mProxyActivated && mProxyAnimateSwitch)) {
            if (original_windows.isEmpty()) {
                // can happen when last window is closed.
                return;
            }
            int index = original_windows.indexOf(effects->currentTabBoxWindow());
            int leftIndex = index;
            int rightIndex = index + 1;
            if (rightIndex == original_windows.count())
                rightIndex = 0;
            EffectWindowList ordered_windows;

            int leftWindowCount = original_windows.count() / 2;
            int rightWindowCount = leftWindowCount;
            if (original_windows.count() % 2 == 1)
                leftWindowCount++;
            for (int i = 0; i < leftWindowCount; i++) {
                int tempIndex = (leftIndex - i);
                if (tempIndex < 0)
                    tempIndex = original_windows.count() + tempIndex;
                ordered_windows.prepend(original_windows[ tempIndex ]);
            }
            for (int i = 0; i < rightWindowCount; i++) {
                int tempIndex = (rightIndex + i) % original_windows.count();
                ordered_windows.append(original_windows[ tempIndex ]);
            }
            // move items cause of schedule
            for (int i = 0; i < scheduled_directions.count(); i++) {
                Direction actual = scheduled_directions[ i ];
                if (actual == Left) {
                    EffectWindow* w = ordered_windows.takeLast();
                    ordered_windows.prepend(w);
                } else {
                    EffectWindow* w = ordered_windows.takeFirst();
                    ordered_windows.append(w);
                }
            }
            if (animation && timeLine.currentValue() < 0.5) {
                if (direction == Left) {
                    EffectWindow* w = ordered_windows.takeLast();
                    edge_window = w;
                    ordered_windows.prepend(w);
                } else {
                    EffectWindow* w = ordered_windows.takeFirst();
                    edge_window = w;
                    ordered_windows.append(w);
                }
            } else if (animation && timeLine.currentValue() >= 0.5) {
                if (animation && direction == Left)
                    edge_window = ordered_windows.last();
                if (animation && direction == Right)
                    edge_window = ordered_windows.first();
            }
            int offset = 0;
            if (animation) {
                if (direction == Left)
                    offset = (float)item_max_size.width() * (1.0 - timeLine.currentValue());
                else
                    offset = -(float)item_max_size.width() * (1.0 - timeLine.currentValue());
            }
            for (int i = 0; i < ordered_windows.count(); i++) {
                if (!ordered_windows.at(i))
                    continue;
                EffectWindow* w = ordered_windows.at(i);
                ItemInfo *info = windows.value(w, 0);
                if (!info)
                    windows[ w ] = info = new ItemInfo();

                info->iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
                info->iconFrame->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                info->iconFrame->setIcon(w->icon());

                float moveIndex = i;
                if (animation && timeLine.currentValue() < 0.5) {
                    if (direction == Left)
                        moveIndex--;
                    else
                        moveIndex++;
                }
                if (ordered_windows.count() % 2 == 0)
                    moveIndex += 0.5;
                info->area = QRect(frame_area.x() + moveIndex * item_max_size.width() + offset,
                                   frame_area.y(),
                                   item_max_size.width(), item_max_size.height());
                info->clickable = info->area;
            }
            if (ordered_windows.count() % 2 == 0) {
                right_window = ordered_windows.last();
            }
            if (!highlight_is_set) {
                ItemInfo *info = windows.value(selected_window, 0);
                highlight_area = info ? info->area : QRect();
                highlight_is_set = true;
            }
        } else {
            for (int i = 0; i < original_windows.count(); i++) {
                if (!original_windows.at(i))
                    continue;
                EffectWindow* w = original_windows.at(i);
                ItemInfo *info = windows.value(w, 0);
                if (!info)
                    windows[ w ] = info = new ItemInfo();

                info->iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
                info->iconFrame->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                info->iconFrame->setIcon(w->icon());

                info->area = QRect(frame_area.x() + i * item_max_size.width(),
                                   frame_area.y(),
                                   item_max_size.width(), item_max_size.height());
                info->clickable = info->area;
            }
        }
    } else {
        desktops.clear();
        for (int i = 0; i < original_desktops.count(); i++) {
            int it = original_desktops.at(i);
            ItemInfo *info = desktops.value( it, 0 );
            if (!info)
                desktops[ it ] = info = new ItemInfo();

            info->area = QRect(frame_area.x() + i * item_max_size.width(),
                               frame_area.y(),
                               item_max_size.width(), item_max_size.height());
            info->clickable = info->area;
        }
    }
}

void BoxSwitchEffect::paintWindowThumbnail(EffectWindow* w)
{
    ItemInfo *info = windows.value(w, 0);
    if (!info)
        return;
    WindowPaintData data(w);

    setPositionTransformations(data,
                               info->thumbnail, w,
                               info->area.adjusted(highlight_margin, highlight_margin, -highlight_margin, -highlight_margin),
                               Qt::KeepAspectRatio);

    if (animation && (w == edge_window) && (windows.size() % 2 == 1)) {
        // in case of animation:
        // the window which has to change the side will be split and painted on both sides of the edge
        double splitPoint = 0.0;
        if (direction == Left) {
            splitPoint = w->geometry().width() * timeLine.currentValue();
        } else {
            splitPoint = w->geometry().width() - w->geometry().width() * timeLine.currentValue();
        }
        data.quads = data.quads.splitAtX(splitPoint);
        WindowQuadList left_quads;
        WindowQuadList right_quads;
        foreach (const WindowQuad & quad, data.quads) {
            if (quad.left() >= splitPoint)
                left_quads << quad;
            if (quad.right() <= splitPoint)
                right_quads << quad;
        }
        // the base position of the window is changed after half of the animation
        if (timeLine.currentValue() < 0.5) {
            if (direction == Left)
                data.quads = left_quads;
            else
                data.quads = right_quads;
        } else {
            if (direction == Left)
                data.quads = right_quads;
            else
                data.quads = left_quads;
        }

        // paint one part of the thumbnail
        effects->paintWindow(w,
                             PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
                             info->thumbnail, data);

        QRect secondThumbnail;

        // paint the second part of the thumbnail:
        // the other window quads are needed and a new rect for transformation has to be calculated
        if (direction == Left) {
            if (timeLine.currentValue() < 0.5) {
                data.quads = right_quads;
                secondThumbnail = QRect(frame_area.x() + frame_area.width() -
                                        (float)item_max_size.width() * timeLine.currentValue(),
                                        frame_area.y(), item_max_size.width(), item_max_size.height());
            } else {
                data.quads = left_quads;
                secondThumbnail = QRect(frame_area.x() - (float)item_max_size.width() * timeLine.currentValue(),
                                        frame_area.y(), item_max_size.width(), item_max_size.height());
                if (right_window)
                    secondThumbnail = QRect(frame_area.x() -
                                            (float)item_max_size.width() * (timeLine.currentValue() - 0.5),
                                            frame_area.y(), item_max_size.width(), item_max_size.height());
            }
        } else {
            if (timeLine.currentValue() < 0.5) {
                data.quads = left_quads;
                secondThumbnail = QRect(frame_area.x() -
                                        (float)item_max_size.width() * (1.0 - timeLine.currentValue()),
                                        frame_area.y(), item_max_size.width(), item_max_size.height());
            } else {
                data.quads = right_quads;
                secondThumbnail = QRect(frame_area.x() + frame_area.width() -
                                        (float)item_max_size.width() * (1.0 - timeLine.currentValue()),
                                        frame_area.y(), item_max_size.width(), item_max_size.height());
            }
        }
        setPositionTransformations(data,
                                   info->thumbnail, w,
                                   secondThumbnail.adjusted(highlight_margin, highlight_margin, -highlight_margin, -highlight_margin),
                                   Qt::KeepAspectRatio);
        effects->paintWindow(w,
                             PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
                             info->thumbnail, data);
    } else if ((windows.size() % 2 == 0) && (w == right_window)) {
        // in case of even number of thumbnails:
        // the window on the right is painted one half on left and on half on the right side
        float animationOffset = 0.0f;
        double splitPoint = w->geometry().width() * 0.5;
        if (animation && timeLine.currentValue() <= 0.5) {
            // in case of animation the right window has only to be split during first half of animation
            if (direction == Left) {
                splitPoint += w->geometry().width() * timeLine.currentValue();
                animationOffset = -(float)item_max_size.width() * timeLine.currentValue();
            } else {
                splitPoint -= w->geometry().width() * timeLine.currentValue();
                animationOffset = (float)item_max_size.width() * timeLine.currentValue();
            }
        }
        if (animation && timeLine.currentValue() > 0.5) {
            // at half of animation a different window has become the right window
            // we have to adjust the splitting again
            if (direction == Left) {
                splitPoint -= w->geometry().width() * (1.0 - timeLine.currentValue());
                animationOffset = (float)item_max_size.width() * (1.0 - timeLine.currentValue());
            } else {
                splitPoint += w->geometry().width() * (1.0 - timeLine.currentValue());
                animationOffset = -(float)item_max_size.width() * (1.0 - timeLine.currentValue());
            }
        }
        data.quads = data.quads.splitAtX(splitPoint);
        WindowQuadList rightQuads;
        WindowQuadList leftQuads;
        foreach (const WindowQuad & quad, data.quads) {
            if (quad.right() <= splitPoint)
                leftQuads << quad;
            else
                rightQuads << quad;
        }

        // left quads are painted on right side of frame
        data.quads = leftQuads;
        effects->drawWindow(w,
                            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
                            info->thumbnail, data);

        // right quads are painted on left side of frame
        data.quads = rightQuads;
        QRect secondThumbnail;
        secondThumbnail = QRect(frame_area.x() -
                                (float)item_max_size.width() * 0.5 + animationOffset,
                                frame_area.y(), item_max_size.width(), item_max_size.height());
        setPositionTransformations(data,
                                   info->thumbnail, w,
                                   secondThumbnail.adjusted(highlight_margin, highlight_margin, -highlight_margin, -highlight_margin),
                                   Qt::KeepAspectRatio);
        effects->drawWindow(w,
                            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
                            info->thumbnail, data);
    } else {
        effects->drawWindow(w,
                            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_LANCZOS,
                            info->thumbnail, data);
    }
}

void BoxSwitchEffect::paintDesktopThumbnail(int iDesktop)
{
    ItemInfo *info = desktops.value(iDesktop, 0);
    if (!info)
        return;

    ScreenPaintData data;
    QRect region;
    QRect r = info->area.adjusted(highlight_margin, highlight_margin, -highlight_margin, -highlight_margin);
    QSize size = QSize(displayWidth(), displayHeight());

    size.scale(r.size(), Qt::KeepAspectRatio);
    data.xScale = size.width() / double(displayWidth());
    data.yScale = size.height() / double(displayHeight());
    int width = int(displayWidth() * data.xScale);
    int height = int(displayHeight() * data.yScale);
    int x = r.x() + (r.width() - width) / 2;
    int y = r.y() + (r.height() - height) / 2;
    region = QRect(x, y, width, height);
    data.xTranslate = x;
    data.yTranslate = y;

    effects->paintScreen(PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST,
                         region, data);
}

void BoxSwitchEffect::paintWindowIcon(EffectWindow* w)
{
    ItemInfo *info = windows.value(w, 0);
    if (!info)
        return;
    // Don't render null icons
    if (w->icon().isNull()) {
        return;
    }

    int width = w->icon().width();
    int height = w->icon().height();
    int x = info->area.x() + info->area.width() - width - highlight_margin;
    int y = info->area.y() + info->area.height() - height - highlight_margin;
    if ((windows.size() % 2 == 0)) {
        if (w == right_window) {
            // in case of right window the icon has to be painted on the left side of the frame
            x = frame_area.x() + info->area.width() * 0.5 - width - highlight_margin;
            if (animation) {
                if (timeLine.currentValue() <= 0.5) {
                    if (direction == Left) {
                        x -= info->area.width() * timeLine.currentValue();
                        x = qMax(x, frame_area.x());
                    } else
                        x += info->area.width() * timeLine.currentValue();
                } else {
                    if (direction == Left)
                        x += info->area.width() * (1.0 - timeLine.currentValue());
                    else {
                        x -= info->area.width() * (1.0 - timeLine.currentValue());
                        x = qMax(x, frame_area.x());
                    }
                }
            }
        }
    } else {
        // during animation the icon of the edge window has to change position
        if (animation && w == edge_window) {
            if (timeLine.currentValue() < 0.5) {
                if (direction == Left)
                    x += info->area.width() * timeLine.currentValue();
                else
                    x -= info->area.width() * timeLine.currentValue();
            } else {
                if (direction == Left)
                    x -= info->area.width() * (1.0 - timeLine.currentValue());
                else
                    x += info->area.width() * (1.0 - timeLine.currentValue());
            }
        }
    }

    info->iconFrame->setPosition(QPoint(x, y));
    info->iconFrame->render(infiniteRegion(), 1.0, 0.75);
}

void* BoxSwitchEffect::proxy()
{
    return &mProxy;
}

void BoxSwitchEffect::activateFromProxy(int mode, bool animate, bool showText, float positioningFactor)
{
    if (!mActivated) {
        mProxyActivated = true;
        mProxyAnimateSwitch = animate;
        mProxyShowText = showText;
        mPositioningFactor = positioningFactor;
        thumbnailFrame->setText(" ");
        if ((mode == TabBoxWindowsMode) || (mode == TabBoxWindowsAlternativeMode)) {
            if (effects->currentTabBoxWindowList().count() > 0) {
                mMode = mode;
                effects->refTabBox();
                highlight_is_set = false;
                animation = false;
                scheduled_directions.clear();
                right_window = 0;
                setActive();
            }
        } else if (mode == TabBoxDesktopListMode || mode == TabBoxDesktopMode) {
            // DesktopMode
            if (effects->currentTabBoxDesktopList().count() > 0) {
                mMode = mode;
                painting_desktop = 0;
                effects->refTabBox();
                setActive();
            }
        }
        if (!mActivated)
            mProxyActivated = false;
    }
}

BoxSwitchEffect::ItemInfo::ItemInfo()
    : iconFrame(NULL)
{
}

BoxSwitchEffect::ItemInfo::~ItemInfo()
{
    delete iconFrame;
}

} // namespace
