/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbufferallocator.h"

namespace KWin
{

class KWIN_EXPORT ShmGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    std::expected<GraphicsBuffer *, std::string> allocate(const GraphicsBufferOptions &options) override;
};

} // namespace KWin
