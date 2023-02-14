/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include <kwin_export.h>

#include <QObject>
#include <QRect>

#include <xcb/randr.h>

namespace KWin
{

class X11StandaloneBackend;

/**
 * X11 output representation
 */
class KWIN_EXPORT X11Output : public Output
{
    Q_OBJECT

public:
    explicit X11Output(X11StandaloneBackend *backend, QObject *parent = nullptr);

    void updateEnabled(bool enabled);

    RenderLoop *renderLoop() const override;
    void setRenderLoop(RenderLoop *loop);

    int xineramaNumber() const;
    void setXineramaNumber(int number);

    bool setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation) override;

private:
    void setCrtc(xcb_randr_crtc_t crtc);
    void setGammaRampSize(int size);

    X11StandaloneBackend *m_backend;
    RenderLoop *m_loop = nullptr;
    xcb_randr_crtc_t m_crtc = XCB_NONE;
    int m_gammaRampSize;
    int m_xineramaNumber = 0;

    friend class X11StandaloneBackend;
};

}
