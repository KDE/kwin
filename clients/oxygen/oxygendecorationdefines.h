#ifndef oxygendecorationdefines_h
#define oxygendecorationdefines_h
//////////////////////////////////////////////////////////////////////////////
// oxygendecorationdefines.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

#include "oxygenconfiguration.h"

#include <QSharedPointer>
#include <QList>

namespace Oxygen
{

    //! this should move to some global declaration
    typedef QSharedPointer<Configuration> ConfigurationPtr;
    typedef QList<ConfigurationPtr> ConfigurationList;
    typedef QListIterator<ConfigurationPtr> ConfigurationListIterator;

    //! buttons
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
        ButtonApplicationMenu,
        ButtonTypeCount,

        // Close only one tab
        ButtonItemClose=100,

        // shows the window menu for one tab
        ButtonItemMenu

    };

    //! buttons status
    enum ButtonStatus {
        Normal = 0,
        Hovered = 1<<0,
        Pressed = 1<<1
    };

    //! exception
    enum ExceptionMask
    {
        None = 0,
        TitleAlignment = 1<<0,
        DrawSeparator = 1<<2,
        TitleOutline = 1<<3,
        FrameBorder = 1<<4,
        BlendColor = 1<<5,
        SizeGripMode = 1<<6
    };

}

#endif
