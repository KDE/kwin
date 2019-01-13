/*
 * Copyright 2019 Martin Flöser <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "windoweffects.h"
#include "effect_builtins.h"
#include "../../effects.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

Q_DECLARE_METATYPE(KWindowEffects::SlideFromLocation)

namespace KWin
{

WindowEffects::WindowEffects()
    : QObject(),
      KWindowEffectsPrivate()
{
}

WindowEffects::~WindowEffects()
{}

namespace
{
QWindow *findWindow(WId win)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win] (QWindow *w) { return w->winId() == win; });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}
}

bool WindowEffects::isEffectAvailable(KWindowEffects::Effect effect)
{
    if (!effects) {
        return false;
    }
    auto e = static_cast<EffectsHandlerImpl*>(effects);
    switch (effect) {
    case KWindowEffects::BackgroundContrast:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::Contrast));
    case KWindowEffects::BlurBehind:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::Blur));
    case KWindowEffects::Slide:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::SlidingPopups));
    default:
        // plugin does not provide integration for other effects
        return false;
    }
}

void WindowEffects::slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset)
{
    auto w = findWindow(id);
    if (!w) {
        return;
    }
    w->setProperty("kwin_slide", QVariant::fromValue(location));
    w->setProperty("kwin_slide_offset", offset);
}

void WindowEffects::slideWindow(QWidget *widget, KWindowEffects::SlideFromLocation location)
{
    slideWindow(widget->winId(), location, 0);
}

QList<QSize> WindowEffects::windowSizes(const QList<WId> &ids)
{
    Q_UNUSED(ids)
    return {};
}

void WindowEffects::presentWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}

void WindowEffects::presentWindows(WId controller, int desktop)
{
    Q_UNUSED(controller)
    Q_UNUSED(desktop)
}

void WindowEffects::highlightWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}

void WindowEffects::enableBlurBehind(WId window, bool enable, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_blur", region);
    } else {
        w->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(WId window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_background_region", region);
        w->setProperty("kwin_background_contrast", contrast);
        w->setProperty("kwin_background_intensity", intensity);
        w->setProperty("kwin_background_saturation", saturation);
    } else {
        w->setProperty("kwin_background_region", {});
        w->setProperty("kwin_background_contrast", {});
        w->setProperty("kwin_background_intensity", {});
        w->setProperty("kwin_background_saturation", {});
    }
}

void WindowEffects::markAsDashboard(WId window)
{
    Q_UNUSED(window)
}

}
