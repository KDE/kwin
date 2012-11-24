/*
  This file is part of the KDE project.

  Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
 */

#include "kcommondecoration_p.h"

#include "kcommondecoration.h"
#include "kdecorationbridge.h"

#include <assert.h>

#include "kcommondecoration_p.moc"

KCommonDecorationWrapper::KCommonDecorationWrapper(KCommonDecoration* deco, KDecorationBridge* bridge, KDecorationFactory* factory)
    : KDecorationUnstable(bridge, factory)
    , decoration(deco)
{
}

KCommonDecorationWrapper::~KCommonDecorationWrapper()
{
    // the wrapper actually owns KCommonDecoration, since the wrapper is what KWin core uses
    delete decoration;
}

void KCommonDecorationWrapper::init()
{
    return decoration->init();
}

KCommonDecorationWrapper::Position KCommonDecorationWrapper::mousePosition(const QPoint& p) const
{
    return decoration->mousePosition(p);
}

void KCommonDecorationWrapper::borders(int& left, int& right, int& top, int& bottom) const
{
    return decoration->borders(left, right, top, bottom);
}

void KCommonDecorationWrapper::resize(const QSize& s)
{
    return decoration->resize(s);
}

QSize KCommonDecorationWrapper::minimumSize() const
{
    return decoration->minimumSize();
}

void KCommonDecorationWrapper::activeChange()
{
    return decoration->activeChange();
}

void KCommonDecorationWrapper::captionChange()
{
    return decoration->captionChange();
}

void KCommonDecorationWrapper::iconChange()
{
    return decoration->iconChange();
}

void KCommonDecorationWrapper::maximizeChange()
{
    return decoration->maximizeChange();
}

void KCommonDecorationWrapper::desktopChange()
{
    return decoration->desktopChange();
}

void KCommonDecorationWrapper::shadeChange()
{
    return decoration->shadeChange();
}

bool KCommonDecorationWrapper::drawbound(const QRect& geom, bool clear)
{
    return decoration->drawbound(geom, clear);
}

bool KCommonDecorationWrapper::windowDocked(Position side)
{
    return decoration->windowDocked(side);
}

void KCommonDecorationWrapper::reset(unsigned long changed)
{
    return decoration->reset(changed);
}

void KCommonDecorationWrapper::padding(int &left, int &right, int &top, int &bottom) const
{
    left   = decoration->layoutMetric(KCommonDecoration::LM_OuterPaddingLeft);
    right  = decoration->layoutMetric(KCommonDecoration::LM_OuterPaddingRight);
    top    = decoration->layoutMetric(KCommonDecoration::LM_OuterPaddingTop);
    bottom = decoration->layoutMetric(KCommonDecoration::LM_OuterPaddingBottom);
}

void KCommonDecorationWrapper::wrapSetAlphaEnabled(bool enabled)
{
    setAlphaEnabled(enabled);
}

QRegion KCommonDecorationWrapper::region(KDecorationDefines::Region r)
{
    QRegion region;
    QMetaObject::invokeMethod(decoration, "region",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRegion, region),
        Q_ARG(KDecorationDefines::Region, r));
    return region;
}
