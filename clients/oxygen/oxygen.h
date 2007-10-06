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

static const int OXYGEN_BUTTONSIZE      = 21;

enum ButtonType {
    ButtonHelp=0,
    ButtonMax,
    ButtonMin,
    ButtonClose,
    ButtonMenu,
    ButtonSticky,
    ButtonTypeCount
};
Q_DECLARE_FLAGS(ButtonTypes, ButtonType)

class OxygenFactory: public KDecorationFactory
{
public:
    OxygenFactory();
    virtual ~OxygenFactory();
    virtual KDecoration *createDecoration(KDecorationBridge *b);
    virtual bool reset(unsigned long changed);

    static bool initialized();
    static Qt::Alignment titleAlign();

private:
    bool readConfig();

private:
    static bool initialized_;
    static Qt::Alignment titlealign_;
};

inline bool OxygenFactory::initialized()
    { return initialized_; }

inline Qt::Alignment OxygenFactory::titleAlign()
    { return titlealign_; }

} //namespace Oxygen

#endif
