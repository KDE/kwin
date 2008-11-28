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
    QPalette palette = qApp->palette();

    // Set palette to the right group. Which is active right now while drawing the glow
    palette.setCurrentColorGroup(QPalette::Active);

    // TODO: THIS IS ALL VERY UGLY! Not recommended to do it this way.
    // Copied from the shadow effect's XRender picture generator

    // TODO: You can add fake anti-aliasing here :)

    QList< QList<QImage> > textureLists;
    QList<QImage> textures;

#define shadowFuzzyness 10
#define shadowSize 10

    //---------------------------------------------------------------
    // Active shadow texture

    QColor color = palette.window().color();
    QColor light = oxygenHelper()->calcLightColor(oxygenHelper()->backgroundTopColor(color));
    QColor dark = oxygenHelper()->calcDarkColor(oxygenHelper()->backgroundBottomColor(color));
    QColor glow = KDecoration::options()->color(ColorFrame);
    QColor glow2 = KDecoration::options()->color(ColorTitleBar);

    qreal size = 25.5;
    QPixmap *shadow = new QPixmap( size*2, size*2 );
    shadow->fill( Qt::transparent );
    QRadialGradient rg( size, size, size );
    QColor c = color;
    c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
    c = glow;
    c.setAlpha( 220 );  rg.setColorAt( 4.5/size, c );
    c.setAlpha( 180 );  rg.setColorAt( 5/size, c );
    c.setAlpha( 25 );  rg.setColorAt( 5.5/size, c );
    c.setAlpha( 0 );  rg.setColorAt( 6.5/size, c );
    QPainter p( shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );

    rg = QRadialGradient( size, size, size );
    c = color;
    c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
    c = glow2;
    c.setAlpha( 0.58*255 );  rg.setColorAt( 4.5/size, c );
    c.setAlpha( 0.43*255 );  rg.setColorAt( 5.5/size, c );
    c.setAlpha( 0.30*255 );  rg.setColorAt( 6.5/size, c );
    c.setAlpha( 0.22*255 );  rg.setColorAt( 7.5/size, c );
    c.setAlpha( 0.15*255 );  rg.setColorAt( 8.5/size, c );
    c.setAlpha( 0.08*255 );  rg.setColorAt( 11.5/size, c );
    c.setAlpha( 0);  rg.setColorAt( 14.5/size, c );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );
    
    // draw the corner of the window - actually all 4 corners as one circle
    p.setBrush( Qt::NoBrush );
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    lg.setColorAt(0.52, light);
    lg.setColorAt(1.0, dark);
    p.setPen(QPen(lg, 0.8));
    p.drawEllipse(QRectF(size-4, size-4, 8, 8));

    // cut out the part of the texture that is under the window
    p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
    p.setBrush( QColor( 0, 0, 0, 255 ));
    p.setPen( Qt::NoPen );
    p.drawEllipse(QRectF(size-3, size-3, 6, 6));
    p.drawRect(QRectF(size-3, size-1, 6, 2));
    p.drawRect(QRectF(size-1, size-3, 2, 6));

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

    MAKE_TEX( w, h, 0, h+1 ); // corner
    MAKE_TEX( 1, h, w, h+1 );
    MAKE_TEX( w, h, w+1, h+1 );// corner
    MAKE_TEX( w, 1, 0, h );
    MAKE_TEX( w, 1, w+1, h );
    MAKE_TEX( w, h, 0, 0);// corner
    MAKE_TEX( 1, h, w, 0);
    MAKE_TEX( w, h, w+1, 0);// corner

    textureLists.append( textures );

    //---------------------------------------------------------------
    // Inactive shadow texture

    textures.clear();

    shadow->fill( Qt::transparent );
    p.begin(shadow);
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );

    rg = QRadialGradient( size, size+4, size );
    c = QColor( Qt::black );
    c.setAlpha( 0.12*255 );  rg.setColorAt( 4.5/size, c );
    c.setAlpha( 0.11*255 );  rg.setColorAt( 6.6/size, c );
    c.setAlpha( 0.075*255 );  rg.setColorAt( 8.5/size, c );
    c.setAlpha( 0.06*255 );  rg.setColorAt( 11.5/size, c );
    c.setAlpha( 0.035*255 );  rg.setColorAt( 14.5/size, c );
    c.setAlpha( 0.025*255 );  rg.setColorAt( 17.5/size, c );
    c.setAlpha( 0.01*255 );  rg.setColorAt( 21.5/size, c );
    c.setAlpha( 0.0*255 );  rg.setColorAt( 25.5/size, c );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );

    rg = QRadialGradient( size, size+2, size );
    c = QColor( Qt::black );
    c.setAlpha( 0.25*255 );  rg.setColorAt( 4.5/size, c );
    c.setAlpha( 0.20*255 );  rg.setColorAt( 5.5/size, c );
    c.setAlpha( 0.13*255 );  rg.setColorAt( 7.5/size, c );
    c.setAlpha( 0.06*255 );  rg.setColorAt( 8.5/size, c );
    c.setAlpha( 0.015*255 );  rg.setColorAt( 11.5/size, c );
    c.setAlpha( 0.0*255 );  rg.setColorAt( 14.5/size, c );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );

    rg = QRadialGradient( size, size+0.2, size );
    c = color;
    c = QColor( Qt::black );
    c.setAlpha( 0.35*255 );  rg.setColorAt( 0/size, c );
    c.setAlpha( 0.32*255 );  rg.setColorAt( 4.5/size, c );
    c.setAlpha( 0.22*255 );  rg.setColorAt( 5.0/size, c );
    c.setAlpha( 0.03*255 );  rg.setColorAt( 5.5/size, c );
    c.setAlpha( 0.0*255 );  rg.setColorAt( 6.5/size, c );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );

    rg = QRadialGradient( size, size, size );
    c = color;
    c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
    c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( rg );
    p.drawRect( shadow->rect() );

    // draw the corner of the window - actually all 4 corners as one circle
    p.setBrush( Qt::NoBrush );
    p.setPen(QPen(lg, 0.8));
    p.drawEllipse(QRectF(size-4, size-4, 8, 8));

    // cut out the part of the texture that is under the window
    p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
    p.setBrush( QColor( 0, 0, 0, 255 ));
    p.setPen( Qt::NoPen );
    p.drawEllipse(QRectF(size-3, size-3, 6, 6));
    p.drawRect(QRectF(size-3, size-1, 6, 2));
    p.drawRect(QRectF(size-1, size-3, 2, 6));

    p.end();

    MAKE_TEX( w, h, 0, h+1 ); // corner
    MAKE_TEX( 1, h, w, h+1 );
    MAKE_TEX( w, h, w+1, h+1 );// corner
    MAKE_TEX( w, 1, 0, h );
    MAKE_TEX( w, 1, w+1, h );
    MAKE_TEX( w, h, 0, 0);// corner
    MAKE_TEX( 1, h, w, 0);
    MAKE_TEX( w, h, w+1, 0);// corner

    textureLists.append( textures );

    delete shadow;

    return textureLists;
}

int OxygenFactory::shadowTextureList( ShadowType type ) const
{
    switch( type ) {
        case ShadowBorderedActive:
            return 0;
        case ShadowBorderedInactive:
            return 1;
    }
    abort(); // Should never be reached
}

QList<QRect> OxygenFactory::shadowQuads( ShadowType type, QSize size ) const
{
    int outside=20, underlap=5, cornersize=25;
    // These are underlap under the decoration so the corners look nicer 10px on the outside
    QList<QRect> quads;
    /*quads.append(QRect(-outside, size.height()-underlap, cornersize, cornersize));
    quads.append(QRect(underlap, size.height()-underlap, size.width()-2*underlap, cornersize));
    quads.append(QRect(size.width()-underlap, size.height()-underlap, cornersize, cornersize));
    quads.append(QRect(-outside, underlap, cornersize, size.height()-2*underlap));
    quads.append(QRect(size.width()-underlap, underlap, cornersize, size.height()-2*underlap));
    quads.append(QRect(-outside, -outside, cornersize, cornersize));
    quads.append(QRect(underlap, -outside, size.width()-2*underlap, cornersize));
    quads.append(QRect(size.width()-underlap,     -outside, cornersize, cornersize));*/
    return quads;
}

double OxygenFactory::shadowOpacity( ShadowType type ) const
{
    return 1.0;
}

} //namespace Oxygen
