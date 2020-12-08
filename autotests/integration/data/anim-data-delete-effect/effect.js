/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
effects.windowAdded.connect(function(w) {
    w.fadeAnimation = effect.animate(w, Effect.Opacity, 100, 1.0, 0.0);
});
effect.animationEnded.connect(function(w) {
    cancel(w.fadeAnimation);
});
