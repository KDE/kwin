/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_X11_OUTPUT_H
#define KWIN_X11_OUTPUT_H

#include "output.h"
#include <kwin_export.h>

#include <QObject>
#include <QRect>

#include <xcb/randr.h>

namespace KWin
{

/**
 * X11 output representation
 */
class KWIN_EXPORT X11Output : public Output
{
    Q_OBJECT

public:
    explicit X11Output(QObject *parent = nullptr);

    bool usesSoftwareCursor() const override;

    RenderLoop *renderLoop() const override;
    void setRenderLoop(RenderLoop *loop);

    int xineramaNumber() const;
    void setXineramaNumber(int number);

    void setColorTransformation(const QSharedPointer<ColorTransformation> &transformation) override;

    void setMode(const QSize &size, int refreshRate);

private:
    void setCrtc(xcb_randr_crtc_t crtc);
    void setGammaRampSize(int size);

    RenderLoop *m_loop = nullptr;
    xcb_randr_crtc_t m_crtc = XCB_NONE;
    int m_gammaRampSize;
    int m_xineramaNumber = 0;

    friend class X11StandalonePlatform;
};

}

#endif
