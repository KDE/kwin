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

extern "C"
{
KDE_EXPORT KDecorationFactory* create_factory()
{
    return new Oxygen::OxygenFactory();
}
}
namespace Oxygen
{


//////////////////////////////////////////////////////////////////////////////
// OxygenFactory Class                                                     //
//////////////////////////////////////////////////////////////////////////////

bool OxygenFactory::initialized_ = false;
Qt::Alignment OxygenFactory::titleAlignment_ = Qt::AlignLeft;
bool OxygenFactory::showStripes_ = true;

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

    bool oldstripes = showStripes;    
    showStripes_ = group.readEntry( "ShowStripes", true );

    if (oldalign == titleAlignment_ && oldstripes == showStripes_)
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
            return true;
        // no colors supported at this time
        default:
            return false;
    };
}

//////////////////////////////////////////////////////////////////////////////
// Shadows

QList< QList<QImage> > OxygenFactory::shadowTextures()
{
    // TODO: THIS IS ALL VERY UGLY! Not recommended to do it this way.
    // Copied from the shadow effect's XRender picture generator

    // TODO: You can add fake anti-aliasing here :)

    QList< QList<QImage> > textureLists;
    QList<QImage> textures;

#define shadowFuzzyness 10
#define shadowSize 10

    //---------------------------------------------------------------
    // Active shadow texture

    qreal size = 2 * ( shadowFuzzyness + shadowSize ) + 1;
    QPixmap *shadow = new QPixmap( size, size );
    shadow->fill( Qt::transparent );
    size /= 2.0;
    QRadialGradient rg( size, size, size );
    QColor c( 150, 234, 255, 255 );
    rg.setColorAt( 0, c );
    c.setAlpha( 0.3 * c.alpha() );
    if( shadowSize > 0 )
        rg.setColorAt( float( shadowSize ) / ( shadowFuzzyness + shadowSize ), c );
    c.setAlpha( 0 );
    rg.setColorAt( 0.8, c );
    QPainter p( shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );
    p.end();

    int w = shadow->width() / 2;
    int h = shadow->height() / 2;
    QPixmap dump;

#define MAKE_TEX( _W_, _H_, _XOFF_, _YOFF_ ) \
    dump = QPixmap( _W_, _H_ ); \
    dump.fill( Qt::transparent ); \
    p.begin( &dump ); \
    p.drawPixmap( 0, 0, *shadow, _XOFF_, _YOFF_, _W_, _H_ ); \
    p.end(); \
    textures.append( dump.toImage() );

    MAKE_TEX( w, h, 0, h );
    MAKE_TEX( 1, h, w, h );
    MAKE_TEX( w, h, w, h );
    MAKE_TEX( w, 1, 0, h );
    //MAKE_TEX( 1, 1, w, h );
    MAKE_TEX( w, 1, w, h );
    MAKE_TEX( w, h, 0, 0 );
    MAKE_TEX( 1, h, w, 0 );
    MAKE_TEX( w, h, w, 0 );
    delete shadow;

    textureLists.append( textures );

    //---------------------------------------------------------------
    // Inactive shadow texture

    for( int i = 0; i < 8; i++ )
        {
        QPainter pi( &textures[i] );
        pi.fillRect( textures[i].rect(), QColor( 0, 0, 0, 255 ));
        pi.end();
        textures[i].setAlphaChannel( textureLists[0][i].alphaChannel() );
        }

    textureLists.append( textures );

    return textureLists;
}

int OxygenFactory::shadowTextureList( ShadowType type ) const
{
    switch( type ) {
        case ShadowBorderedActive:
        case ShadowBorderlessActive:
            return 0;
        case ShadowBorderedInactive:
        case ShadowBorderlessInactive:
        case ShadowOther:
            return 1;
    }
    abort(); // Should never be reached
}

QList<QRect> OxygenFactory::shadowQuads( ShadowType type, QSize size ) const
{
#define shadowFuzzyness 15

    // These are slightly under the decoration so the corners look nicer
    QList<QRect> quads;
    quads.append( QRect( -shadowFuzzyness+5, -shadowFuzzyness+5, shadowFuzzyness, shadowFuzzyness ));
    quads.append( QRect( 0+5,                -shadowFuzzyness+5, size.width()-10, shadowFuzzyness ));
    quads.append( QRect( size.width()-5,     -shadowFuzzyness+5, shadowFuzzyness, shadowFuzzyness ));
    quads.append( QRect( -shadowFuzzyness+5, 0+5,                shadowFuzzyness, size.height()-10 ));
    //quads.append( QRect( 0+5,                0+5,                size.width()-10, size.height()-10 ));
    quads.append( QRect( size.width()-5,     0+5,                shadowFuzzyness, size.height()-10 ));
    quads.append( QRect( -shadowFuzzyness+5, size.height()-5,    shadowFuzzyness, shadowFuzzyness ));
    quads.append( QRect( 0+5,                size.height()-5,    size.width()-10, shadowFuzzyness ));
    quads.append( QRect( size.width()-5,     size.height()-5,    shadowFuzzyness, shadowFuzzyness ));

    return quads;
}

double OxygenFactory::shadowOpacity( ShadowType type, double dataOpacity ) const
{
    switch( type ) {
        case ShadowBorderlessActive:
            return dataOpacity;
        case ShadowBorderlessInactive:
        case ShadowOther:
            return dataOpacity * 0.25;
    }
    abort(); // Should never be reached
}

} //namespace Oxygen
