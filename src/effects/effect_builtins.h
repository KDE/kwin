/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EFFECT_BUILTINS_H
#define KWIN_EFFECT_BUILTINS_H

#define KWIN_IMPORT_BUILTIN_EFFECTS                                                       \
    Q_IMPORT_PLUGIN(BlurEffectFactory)                                                    \
    Q_IMPORT_PLUGIN(ColorPickerEffectFactory)                                             \
    Q_IMPORT_PLUGIN(ContrastEffectFactory)                                                \
    Q_IMPORT_PLUGIN(DesktopGridEffectFactory)                                             \
    Q_IMPORT_PLUGIN(DimInactiveEffectFactory)                                             \
    Q_IMPORT_PLUGIN(FallApartEffectFactory)                                               \
    Q_IMPORT_PLUGIN(GlideEffectFactory)                                                   \
    Q_IMPORT_PLUGIN(HighlightWindowEffectFactory)                                         \
    Q_IMPORT_PLUGIN(InvertEffectFactory)                                                  \
    Q_IMPORT_PLUGIN(KscreenEffectFactory)                                                 \
    Q_IMPORT_PLUGIN(LookingGlassEffectFactory)                                            \
    Q_IMPORT_PLUGIN(MagicLampEffectFactory)                                               \
    Q_IMPORT_PLUGIN(MagnifierEffectFactory)                                               \
    Q_IMPORT_PLUGIN(MouseClickEffectFactory)                                              \
    Q_IMPORT_PLUGIN(MouseMarkEffectFactory)                                               \
    Q_IMPORT_PLUGIN(OverviewEffectFactory)                                                \
    Q_IMPORT_PLUGIN(PresentWindowsEffectFactory)                                          \
    Q_IMPORT_PLUGIN(ResizeEffectFactory)                                                  \
    Q_IMPORT_PLUGIN(ScreenEdgeEffectFactory)                                              \
    Q_IMPORT_PLUGIN(ScreenShotEffectFactory)                                              \
    Q_IMPORT_PLUGIN(ScreenTransformEffectFactory)                                         \
    Q_IMPORT_PLUGIN(SheetEffectFactory)                                                   \
    Q_IMPORT_PLUGIN(ShowFpsEffectFactory)                                                 \
    Q_IMPORT_PLUGIN(ShowPaintEffectFactory)                                               \
    Q_IMPORT_PLUGIN(SlideEffectFactory)                                                   \
    Q_IMPORT_PLUGIN(SlideBackEffectFactory)                                               \
    Q_IMPORT_PLUGIN(SlidingPopupsEffectFactory)                                           \
    Q_IMPORT_PLUGIN(SnapHelperEffectFactory)                                              \
    Q_IMPORT_PLUGIN(StartupFeedbackEffectFactory)                                         \
    Q_IMPORT_PLUGIN(ThumbnailAsideEffectFactory)                                          \
    Q_IMPORT_PLUGIN(TouchPointsEffectFactory)                                             \
    Q_IMPORT_PLUGIN(TrackMouseEffectFactory)                                              \
    Q_IMPORT_PLUGIN(WindowGeometryFactory)                                                \
    Q_IMPORT_PLUGIN(WobblyWindowsEffectFactory)                                           \
    Q_IMPORT_PLUGIN(ZoomEffectFactory)

#endif
