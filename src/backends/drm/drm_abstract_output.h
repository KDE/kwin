/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

namespace KWin
{

class DrmBackend;
class DrmOutputLayer;
class OutputFrame;

class DrmAbstractOutput : public Output
{
    Q_OBJECT
public:
    explicit DrmAbstractOutput();

    RenderLoop *renderLoop() const override;

    virtual DrmOutputLayer *primaryLayer() const = 0;
    virtual DrmOutputLayer *cursorLayer() const = 0;
    virtual DrmOutputLayer *overlayLayer() const;

protected:
    friend class DrmGpu;

    std::unique_ptr<RenderLoop> m_renderLoop;
};

}
