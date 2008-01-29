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

#ifndef KCOMMONDECORATION_P_H
#define KCOMMONDECORATION_P_H

#include "kdecoration.h"

//
// This header file is internal. I mean it.
// 

class KCommonDecoration;
class KDecorationBridge;
class KDecorationFactory;

// wrapper all functionality that needs reimplementing in KDecoration and forward it to KCommonDecoration
class KCommonDecorationWrapper
    : public KDecoration
    {
    Q_OBJECT
    public:
        KCommonDecorationWrapper( KCommonDecoration* deco, KDecorationBridge* bridge, KDecorationFactory* factory );
        virtual ~KCommonDecorationWrapper();
        virtual void init();
        virtual Position mousePosition( const QPoint& p ) const;
        virtual void borders( int& left, int& right, int& top, int& bottom ) const;
        virtual void resize( const QSize& s );
        virtual QSize minimumSize() const;
        virtual void activeChange();
        virtual void captionChange();
        virtual void iconChange();
        virtual void maximizeChange();
        virtual void desktopChange();
        virtual void shadeChange();
        virtual bool drawbound( const QRect& geom, bool clear );
        virtual bool windowDocked( Position side );
        virtual void reset( unsigned long changed );
    private:
        KCommonDecoration* decoration;
    };

#endif // KCOMMONDECORATION_P_H
