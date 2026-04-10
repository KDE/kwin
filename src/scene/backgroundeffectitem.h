/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "scene/effectitem.h"

namespace KWin
{

class WindowItem;

/**
 * A helper item for background effects like blur. It represents the
 * effect in the scene and expands the repaint region when needed.
 */
class KWIN_EXPORT BackgroundEffectItem : public EffectItem
{
    Q_OBJECT

public:
    explicit BackgroundEffectItem(WindowItem *parentItem);

    void setEffectBoundingRect(const RectF &boundingRect);

    uint32_t pixelsToExpandRepaintsBelowOpaqueRegions() const;
    void setPixelsToExpandRepaintsBelowOpaqueRegions(uint32_t pixels);

private:
    void updateGeometry();

    uint32_t m_pixelsToExpandRepaintsBelowOpaqueRegions = 0;
    WindowItem *const m_windowItem;
    RectF m_effectBounds;
};

}
