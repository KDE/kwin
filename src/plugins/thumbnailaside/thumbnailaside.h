/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 Testing of painting a window more than once.

*/

#pragma once

#include "effect/effect.h"
#include "scene/mirroritem.h"

#include <QHash>

namespace KWin
{

class ThumbnailAsideEffect : public Effect
{
    Q_OBJECT

public:
    ThumbnailAsideEffect();
    void reconfigure(ReconfigureFlags) override;

    bool isActive() const override;

private Q_SLOTS:
    void toggleCurrentThumbnail();
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const RectF &old);
    void handleScreenLockingChanged();

private:
    void addThumbnail(EffectWindow *w);
    void removeThumbnail(EffectWindow *w);
    void arrange();

    struct Data
    {
        int index;
        std::unique_ptr<MirrorItem> item;
    };
    std::unordered_map<EffectWindow *, Data> m_windows;
    int m_maxWidth;
    int m_spacing;
    double m_opacity;
    int m_screen;
    Region m_painted;
};

} // namespace
