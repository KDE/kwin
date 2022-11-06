/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

namespace KWin
{

class InternalWindow;
struct InternalWindowFrame;

/**
 * The SurfaceItemInternal class represents an internal surface in the scene.
 */
class KWIN_EXPORT SurfaceItemInternal : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemInternal(InternalWindow *window, Item *parent = nullptr);

    InternalWindow *window() const;

    RegionF shape() const override;

private Q_SLOTS:
    void handlePresented(const InternalWindowFrame &frame);

private:
    InternalWindow *m_window;
};

} // namespace KWin
