//////////////////////////////////////////////////////////////////////////////
// oxygenbutton.h
// -------------------
// Oxygen window decoration for KDE.
// -------------------
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
#ifndef OXYGEN_H
#define OXYGEN_H

#include <kconfig.h>
#include <kdecorationfactory.h>

namespace Oxygen
{
// OxygenFactory /////////////////////////////////////////////////////////////

static const int OXYGEN_BUTTONSIZE      = 22;
#define TFRAMESIZE 3
#define BFRAMESIZE 5
#define LFRAMESIZE 5
#define RFRAMESIZE 5

enum ButtonType {
    ButtonHelp=0,
    ButtonMax,
    ButtonMin,
    ButtonClose,
    ButtonMenu,
    ButtonSticky,
    ButtonAbove,
    ButtonBelow,
    ButtonShade,
    ButtonTypeCount
};
Q_DECLARE_FLAGS(ButtonTypes, ButtonType)

class OxygenFactory: public KDecorationFactoryUnstable
{
public:
    OxygenFactory();
    virtual ~OxygenFactory();
    virtual KDecoration *createDecoration(KDecorationBridge *b);
    virtual bool reset(unsigned long changed);
    virtual bool supports( Ability ability ) const;

    virtual QList< QList<QImage> > shadowTextures();
    virtual int shadowTextureList( ShadowType type ) const;
    virtual QList<QRect> shadowQuads( ShadowType type, QSize size ) const;
    virtual double shadowOpacity( ShadowType type ) const;

    static bool initialized();
    static Qt::Alignment titleAlignment();
    static bool showStripes();
    static bool thinBorders();

private:
    bool readConfig();

private:
    static bool initialized_;
    static Qt::Alignment titleAlignment_;
    static bool showStripes_;
    static bool thinBorders_;
};

inline bool OxygenFactory::initialized()
    { return initialized_; }

inline Qt::Alignment OxygenFactory::titleAlignment()
    { return titleAlignment_; }

inline bool OxygenFactory::showStripes()
    { return showStripes_; }

inline bool OxygenFactory::thinBorders()
    { return thinBorders_; }

} //namespace Oxygen

#endif
