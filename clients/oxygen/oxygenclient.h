//////////////////////////////////////////////////////////////////////////////
// oxygenclient.h
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2003, 2004 David Johnson <david@usermode.org>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#ifndef OXYGENCLIENT_H
#define OXYGENCLIENT_H


#include <kcommondecoration.h>

#include "lib/helper.h"

class QPoint;

namespace Oxygen {

class OxygenClient : public KCommonDecoration
{
    Q_OBJECT
public:
    OxygenClient(KDecorationBridge *b, KDecorationFactory *f);
    virtual ~OxygenClient();

    virtual QString visibleName() const;
    virtual KCommonDecorationButton *createButton(::ButtonType type);
    virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
    virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
    virtual void updateWindowShape();
    virtual void init();

private:
    void paintEvent(QPaintEvent *e);
    void drawScratch(QPainter *p, QPalette &palette, const int start, const int end, const int topMargin);
    QColor titlebarTextColor(const QPalette &palette);
    bool colorCacheInvalid_;
    QColor cachedTitlebarTextColor_;

protected:
    friend class OxygenButton;
    OxygenHelper &helper_;
};


} // namespace Oxygen

#endif // EXAMPLECLIENT_H
