//////////////////////////////////////////////////////////////////////////////
// oxygenbutton.h
// -------------------
// Oxygen window decoration for KDE.
// -------------------
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
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

#include <QPainter>

#include "oxygen.h"
#include "oxygenclient.h"
#include <kconfiggroup.h>
#include <QApplication>

extern "C"
{
KDE_EXPORT KDecorationFactory* create_factory()
{
    return new Oxygen::OxygenFactory();
}
}
namespace Oxygen
{

OxygenHelper *oxygenHelper(); // referenced from definition in oxygendclient.cpp


//////////////////////////////////////////////////////////////////////////////
// OxygenFactory Class                                                     //
//////////////////////////////////////////////////////////////////////////////

bool OxygenFactory::initialized_ = false;
Qt::Alignment OxygenFactory::titleAlignment_ = Qt::AlignLeft;
bool OxygenFactory::showStripes_ = true;
int OxygenFactory::borderSize_ = 4; // BorderSize::BorderNormal

//////////////////////////////////////////////////////////////////////////////
// OxygenFactory()
// ----------------
// Constructor

OxygenFactory::OxygenFactory()
{
    readConfig();
    initialized_ = true;
}

//////////////////////////////////////////////////////////////////////////////
// ~OxygenFactory()
// -----------------
// Destructor

OxygenFactory::~OxygenFactory() { initialized_ = false; }

//////////////////////////////////////////////////////////////////////////////
// createDecoration()
// -----------------
// Create the decoration

KDecoration* OxygenFactory::createDecoration(KDecorationBridge* b)
{
    return (new OxygenClient(b, this))->decoration();
}

//////////////////////////////////////////////////////////////////////////////
// reset()
// -------
// Reset the handler. Returns true if decorations need to be remade, false if
// only a repaint is necessary

bool OxygenFactory::reset(unsigned long changed)
{
    // read in the configuration
    initialized_ = false;
    bool confchange = readConfig();
    initialized_ = true;

    if (confchange ||
        (changed & (SettingDecoration | SettingButtons | SettingBorder))) {
        return true;
    } else {
        resetDecorations(changed);
        return false;
    }

    // taken from plastik
    switch(KDecoration::options()->preferredBorderSize( this )) {
    case BorderTiny:
        borderSize_ = 2;
        break;
    case BorderLarge:
        borderSize_ = 8;
        break;
    case BorderVeryLarge:
        borderSize_ = 12;
        break;
    case BorderHuge:
        borderSize_ = 18;
        break;
    case BorderVeryHuge:
        borderSize_ = 27;
        break;
    case BorderOversized:
        borderSize_ = 40;
        break;
    case BorderNormal:
    default:
        borderSize_ = 4;
    }
}

//////////////////////////////////////////////////////////////////////////////
// readConfig()
// ------------
// Read in the configuration file

bool OxygenFactory::readConfig()
{
    // create a config object
    KConfig config("oxygenrc");
    KConfigGroup group = config.group("Windeco");

    // grab settings
    Qt::Alignment oldalign = titleAlignment_;
    QString value = group.readEntry("TitleAlignment", "Left");
    if (value == "Left")
        titleAlignment_ = Qt::AlignLeft;
    else if (value == "Center")
        titleAlignment_ = Qt::AlignHCenter;
    else if (value == "Right")
        titleAlignment_ = Qt::AlignRight;

    int oldBorderSize = borderSize_;
    switch(KDecoration::options()->preferredBorderSize( this )) {
    case BorderTiny:
        borderSize_ = 2;
        break;
    case BorderLarge:
        borderSize_ = 8;
        break;
    case BorderVeryLarge:
        borderSize_ = 12;
        break;
    case BorderHuge:
        borderSize_ = 18;
        break;
    case BorderVeryHuge:
        borderSize_ = 27;
        break;
    case BorderOversized:
        borderSize_ = 40;
        break;
    case BorderNormal:
    default:
        borderSize_ = 4;
    }

    bool oldstripes = showStripes_;    
    showStripes_ = group.readEntry( "ShowStripes", true );

    if (oldalign == titleAlignment_ && oldstripes == showStripes_
            && oldBorderSize == borderSize_)
        return false;
    else
        return true;
}

bool OxygenFactory::supports( Ability ability ) const
{
    switch( ability ) {
        // announce
        case AbilityAnnounceButtons:
        case AbilityAnnounceColors:
        // buttons
        case AbilityButtonMenu:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonSpacer:
        case AbilityButtonShade:
        // compositing
        case AbilityCompositingShadow: // TODO: UI option to use default shadows instead
        case AbilityUsesAlphaChannel:
            return true;
        // no colors supported at this time
        default:
            return false;
    };
}

QList< OxygenFactory::BorderSize >
OxygenFactory::borderSizes() const
{
    // the list must be sorted
    return QList< BorderSize >() << BorderTiny << BorderNormal <<
	BorderLarge << BorderVeryLarge <<  BorderHuge <<
	BorderVeryHuge << BorderOversized;
}

} //namespace Oxygen
