/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "cube.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kwinconfig.h>
#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <QColor>
#include <QRect>
#include <QEvent>
#include <QKeyEvent>

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( cube, CubeEffect )
KWIN_EFFECT_SUPPORTED( cube, CubeEffect::supported() )

CubeEffect::CubeEffect()
    : activated( false )
    , cube_painting( false )
    , keyboard_grab( false )
    , schedule_close( false )
    , borderActivate( ElectricNone )
    , frontDesktop( 0 )
    , cubeOpacity( 1.0 )
    , opacityDesktopOnly( true )
    , displayDesktopName( false )
    , reflection( true )
    , rotating( false )
    , desktopChangedWhileRotating( false )
    , paintCaps( true )
    , rotationDirection( Left )
    , verticalRotationDirection( Upwards )
    , verticalPosition( Normal )
    , wallpaper( NULL )
    , texturedCaps( true )
    , capTexture( NULL )
    , manualAngle( 0.0 )
    , manualVerticalAngle( 0.0 )
    , currentShape( TimeLine::EaseInOutCurve )
    , start( false )
    , stop( false )
    , reflectionPainting( false )
    , slide( false )
    , oldDesktop( 0 )
    , activeScreen( 0 )
    , animateDesktopChange( false )
    , bigCube( false )
    , bottomCap( false )
    , closeOnMouseRelease( false )
    , zoom( 0.0 )
    , zPosition( 0.0 )
    , useForTabBox( false )
    , tabBoxMode( false )
    , shortcutsRegistered( false )
    , capListCreated( false )
    , recompileList( true )
    , glList( 0 )
    {
    reconfigure( ReconfigureAll );
    }

bool CubeEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void CubeEffect::reconfigure( ReconfigureFlags )
    {
    loadConfig( "Cube" );
    }

void CubeEffect::loadConfig( QString config )
    {
    KConfigGroup conf = effects->effectConfig( config );
    effects->unreserveElectricBorder( borderActivate );
    borderActivate = (ElectricBorder)conf.readEntry( "BorderActivate", (int)ElectricNone );
    effects->reserveElectricBorder( borderActivate );

    cubeOpacity = (float)conf.readEntry( "Opacity", 80 )/100.0f;
    opacityDesktopOnly = conf.readEntry( "OpacityDesktopOnly", false );
    displayDesktopName = conf.readEntry( "DisplayDesktopName", true );
    reflection = conf.readEntry( "Reflection", true );
    rotationDuration = animationTime( conf, "RotationDuration", 500 );
    backgroundColor = conf.readEntry( "BackgroundColor", QColor( Qt::black ) );
    animateDesktopChange = conf.readEntry( "AnimateDesktopChange", false );
    bigCube = conf.readEntry( "BigCube", false );
    // different settings for cylinder and sphere
    if( config == "Cylinder" )
        {
        animateDesktopChange = false;
        bigCube = true;
        }
    if( config == "Sphere" )
        {
        animateDesktopChange = false;
        bigCube = true;
        reflection = false;
        }
    capColor = conf.readEntry( "CapColor", KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    paintCaps = conf.readEntry( "Caps", true );
    closeOnMouseRelease = conf.readEntry( "CloseOnMouseRelease", false );
    float defaultZPosition = 100.0f;
    if( config == "Sphere" )
        defaultZPosition = 450.0f;
    zPosition = conf.readEntry( "ZPosition", defaultZPosition );
    useForTabBox = conf.readEntry( "TabBox", false );
    invertKeys = conf.readEntry( "InvertKeys", false );
    invertMouse = conf.readEntry( "InvertMouse", false );
    dontSlidePanels = conf.readEntry( "DontSlidePanels", true );
    dontSlideStickyWindows = conf.readEntry( "DontSlideStickyWindows", true );
    QString file = conf.readEntry( "Wallpaper", QString("") );
    if( wallpaper )
        wallpaper->discard();
    delete wallpaper;
    wallpaper = NULL;
    if( !file.isEmpty() )
        {
        QImage img = QImage( file );
        if( !img.isNull() )
            {
            wallpaper = new GLTexture( img );
            }
        }
    delete capTexture;
    capTexture = NULL;
    texturedCaps = conf.readEntry( "TexturedCaps", true );
    if( texturedCaps )
        {
        QString capPath = conf.readEntry( "CapPath", KGlobal::dirs()->findResource( "appdata", "cubecap.png" ) );
        QImage img = QImage( capPath );
        if( !img.isNull() )
            {
            // change the alpha value of each pixel
            for( int x=0; x<img.width(); x++ )
                {
                for( int y=0; y<img.height(); y++ )
                    {
                    QRgb pixel = img.pixel( x, y );
                    if( x == 0 || x == img.width()-1 || y == 0 || y == img.height()-1 )
                        img.setPixel( x, y, qRgba( capColor.red(), capColor.green(), capColor.blue(), 255*cubeOpacity ) );
                    else
                        {
                        if( qAlpha(pixel) < 255 )
                            {
                            // Pixel is transparent - has to be blended with cap color
                            int red   = (qAlpha(pixel)/255)*(qRed(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.red();
                            int green = (qAlpha(pixel)/255)*(qGreen(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.green();
                            int blue  = (qAlpha(pixel)/255)*(qBlue(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.blue();
                            int alpha = qAlpha(pixel) + (255 - qAlpha(pixel))*cubeOpacity;
                            img.setPixel( x, y, qRgba( red, green, blue, alpha ) );
                            }
                        else
                            {
                            img.setPixel( x, y, qRgba( qRed(pixel), qGreen(pixel), qBlue(pixel), ((float)qAlpha(pixel))*cubeOpacity ) );
                            }
                        }
                    }
                }
            capTexture = new GLTexture( img );
            capTexture->setFilter( GL_LINEAR );
            capTexture->setWrapMode( GL_CLAMP_TO_EDGE );
            }
        }

    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( rotationDuration );

    verticalTimeLine.setCurveShape( TimeLine::EaseInOutCurve );
    verticalTimeLine.setDuration( rotationDuration );
    slideTimeLine.setCurveShape( TimeLine::EaseInOutCurve );
    slideTimeLine.setDuration( rotationDuration );

    // do not connect the shortcut if we use cylinder or sphere
    KActionCollection* actionCollection = new KActionCollection( this );
    if( config == "Cube" && !shortcutsRegistered )
        {
        KAction* a = static_cast< KAction* >( actionCollection->addAction( "Cube" ));
        a->setText( i18n("Desktop Cube" ));
        a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F11 ));
        connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
        shortcutsRegistered = true;
        }
    if( config == "Cylinder" && !shortcutsRegistered )
        {
        KAction* a = static_cast< KAction* >( actionCollection->addAction( "Cylinder" ));
        a->setText( i18n("Desktop Cylinder" ));
        a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::SHIFT + Qt::Key_F11 ));
        connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
        shortcutsRegistered = true;
        }
    if( config == "Sphere" && !shortcutsRegistered )
        {
        KAction* a = static_cast< KAction* >( actionCollection->addAction( "Sphere" ));
        a->setText( i18n("Desktop Sphere" ));
        a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_F11 ));
        connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
        shortcutsRegistered = true;
        }
    }

CubeEffect::~CubeEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    delete wallpaper;
    delete capTexture;
    }

void CubeEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( activated )
        {
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;

        if( rotating || start || stop )
            {
            timeLine.addTime( time );
            recompileList = true;
            }
        if( verticalRotating )
            {
            verticalTimeLine.addTime( time );
            recompileList = true;
            }
        if( slide && !slideRotations.empty() )
            {
            slideTimeLine.addTime( time );
            recompileList = true;
            if( dontSlidePanels )
                panels.clear();
            if( dontSlideStickyWindows )
                stickyWindows.clear();
            }
        }
    effects->prePaintScreen( data, time );
    }

void CubeEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( activated )
        {
        if( recompileList )
            {
            recompileList = false;
            glPushMatrix();
            glNewList( glList, GL_COMPILE );
            if( !slide )
                rotateCube();
            glEndList();
            glPopMatrix();
            }

        // compile List for cube
        glNewList( glList + 1, GL_COMPILE );
        glPushMatrix();
        if( slide )
            paintSlideCube( mask, region, data );
        else
            paintCube( mask, region, data );
        glPopMatrix();
        glEndList();

        QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
        if( effects->numScreens() > 1 && (slide || bigCube ) )
            rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
        QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );

        // background
        float clearColor[4];
        glGetFloatv( GL_COLOR_CLEAR_VALUE, clearColor );
        glClearColor( backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), 1.0 );
        glClear( GL_COLOR_BUFFER_BIT );
        glClearColor( clearColor[0], clearColor[1], clearColor[2], clearColor[3] );

        // wallpaper
        if( wallpaper && !slide )
            {            
            wallpaper->bind();
            wallpaper->render( region, rect );
            wallpaper->unbind();
            }

        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        if( effects->numScreens() > 1 && !slide && !bigCube )
            {
            windowsOnOtherScreens.clear();
            // unfortunatelly we have to change the projection matrix in dual screen mode
            glMatrixMode( GL_PROJECTION );
            glPushMatrix();
            glLoadIdentity();
            float fovy = 60.0f;
            float aspect = 1.0f;
            float zNear = 0.1f;
            float zFar = 100.0f;
            float ymax = zNear * tan( fovy  * M_PI / 360.0f );
            float ymin = -ymax;
            float xmin =  ymin * aspect;
            float xmax = ymax * aspect;
            float xTranslate = 0.0;
            float yTranslate = 0.0;
            float xminFactor = 1.0;
            float xmaxFactor = 1.0;
            float yminFactor = 1.0;
            float ymaxFactor = 1.0;
            if( rect.x() == 0 && rect.width() != fullRect.width() )
                {
                // horizontal layout: left screen
                xminFactor = (float)rect.width()/(float)fullRect.width();
                xmaxFactor = ((float)fullRect.width()-(float)rect.width()*0.5f)/((float)fullRect.width()*0.5f);
                xTranslate = (float)fullRect.width()*0.5f-(float)rect.width()*0.5f;
                }
            if( rect.x() != 0 && rect.width() != fullRect.width() )
                {
                // horizontal layout: right screen
                xminFactor = ((float)fullRect.width()-(float)rect.width()*0.5f)/((float)fullRect.width()*0.5f);
                xmaxFactor = (float)rect.width()/(float)fullRect.width();
                xTranslate = (float)fullRect.width()*0.5f-(float)rect.width()*0.5f;
                }
            if( rect.y() == 0 && rect.height() != fullRect.height() )
                {
                // vertical layout: top screen
                yminFactor = ((float)fullRect.height()-(float)rect.height()*0.5f)/((float)fullRect.height()*0.5f);
                ymaxFactor = (float)rect.height()/(float)fullRect.height();
                yTranslate = (float)fullRect.height()*0.5f-(float)rect.height()*0.5f;
                }
            if( rect.y() != 0 && rect.height() != fullRect.height() )
                {
                // vertical layout: bottom screen
                yminFactor = (float)rect.height()/(float)fullRect.height();
                ymaxFactor = ((float)fullRect.height()-(float)rect.height()*0.5f)/((float)fullRect.height()*0.5f);
                yTranslate = (float)fullRect.height()*0.5f-(float)rect.height()*0.5f;
                }
            glFrustum( xmin*xminFactor, xmax*xmaxFactor, ymin*yminFactor, ymax*ymaxFactor, zNear, zFar );
            glMatrixMode( GL_MODELVIEW );
            glPushMatrix();
            glTranslatef( xTranslate, yTranslate, 0.0 );
            }

        // some veriables needed for painting the caps
        float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f);
        float point = rect.width()/2*tan(cubeAngle*0.5f*M_PI/180.0f);
        float zTranslate = zPosition + zoom;
        if( start )
            zTranslate *= timeLine.value();
        if( stop )
            zTranslate *= ( 1.0 - timeLine.value() );
        // reflection
        if( reflection && (!slide) )
            {
            // restrict painting the reflections to the current screen
            PaintClipper::push( QRegion( rect ));
            glPushMatrix();
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            // as we restrict with a PaintClipper painting on the current screen
            float scaleFactor = 1000000 * tan( 60.0 * M_PI / 360.0f )/rect.height();
            glScalef( 1.0, -1.0, 1.0 );
            glTranslatef( 0.0, -rect.height()*2, 0.0 );

            glEnable( GL_CLIP_PLANE0 );
            reflectionPainting = true;
            glEnable( GL_CULL_FACE );
            // caps
            if( paintCaps && ( effects->numberOfDesktops() >= 2 ) )
                {
                glPushMatrix();
                glCallList( glList );
                glTranslatef( rect.width()/2, 0.0, -point-zTranslate );
                glRotatef( (1-frontDesktop)*360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0 );
                glTranslatef( 0.0, rect.height(), 0.0 );
                glCullFace( GL_FRONT );
                glCallList( glList + 2 );
                glTranslatef( 0.0, -rect.height(), 0.0 );
                glCullFace( GL_BACK );
                glCallList( glList + 2 );
                glPopMatrix();
                }

            // cube
            glCullFace( GL_FRONT );
            glPushMatrix();
            glCallList( glList );
            glCallList( glList + 1 );
            glPopMatrix();
            glCullFace( GL_BACK );
            glPushMatrix();
            glCallList( glList );
            glCallList( glList + 1 );
            glPopMatrix();

            // cap
            if( paintCaps && ( effects->numberOfDesktops() >= 2 ) )
                {
                glPushMatrix();
                glCallList( glList );
                glTranslatef( rect.width()/2, 0.0, -point-zTranslate );
                glRotatef( (1-frontDesktop)*360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0 );
                glTranslatef( 0.0, rect.height(), 0.0 );
                glCullFace( GL_BACK );
                glCallList( glList + 2 );
                glTranslatef( 0.0, -rect.height(), 0.0 );
                glCullFace( GL_FRONT );
                glCallList( glList + 2 );
                glPopMatrix();
                }
            glDisable( GL_CULL_FACE );
            reflectionPainting = false;
            glDisable( GL_CLIP_PLANE0 );

            glPopMatrix();
            glPushMatrix();
            if( effects->numScreens() > 1 && rect.x() != fullRect.x() && !slide && !bigCube )
                {
                // have to change the reflection area in horizontal layout and right screen
                glTranslatef( -rect.x(), 0.0, 0.0 );
                }
            glTranslatef( rect.x() + rect.width()*0.5f, 0.0, 0.0 );
            float vertices[] = {
                -rect.width()*0.5f, rect.height(), 0.0,
                rect.width()*0.5f, rect.height(), 0.0,
                (float)rect.width()*scaleFactor, rect.height(), -5000,
                -(float)rect.width()*scaleFactor, rect.height(), -5000 };
            // foreground
            float alpha = 0.7;
            if( start )
                alpha = 0.3 + 0.4 * timeLine.value();
            if( stop )
                alpha = 0.3 + 0.4 * ( 1.0 - timeLine.value() );
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glBegin( GL_POLYGON );
            glVertex3f( vertices[0], vertices[1], vertices[2] );
            glVertex3f( vertices[3], vertices[4], vertices[5] );
            // rearground
            alpha = -1.0;
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glVertex3f( vertices[6], vertices[7], vertices[8] );
            glVertex3f( vertices[9], vertices[10], vertices[11] );
            glEnd();
            glPopMatrix();
            PaintClipper::pop( QRegion( rect ));
            }
        glEnable( GL_CULL_FACE );
        // caps
        if( paintCaps && ( effects->numberOfDesktops() >= 2 ) && !slide )
            {
            glPushMatrix();
            glCallList( glList );
            glTranslatef( rect.width()/2, 0.0, -point-zTranslate );
            glRotatef( (1-frontDesktop)*360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0 );
            glTranslatef( 0.0, rect.height(), 0.0 );
            glCullFace( GL_BACK );
            glCallList( glList + 2 );
            glTranslatef( 0.0, -rect.height(), 0.0 );
            glCullFace( GL_FRONT );
            glCallList( glList + 2 );
            glPopMatrix();
            }

        // cube
        glCullFace( GL_BACK );
        glPushMatrix();
        glCallList( glList );
        glCallList( glList + 1 );
        glPopMatrix();
        glCullFace( GL_FRONT );
        glPushMatrix();
        glCallList( glList );
        glCallList( glList + 1 );
        glPopMatrix();

        // cap
        if( paintCaps && ( effects->numberOfDesktops() >= 2 ) && !slide )
            {
            glPushMatrix();
            glCallList( glList );
            glTranslatef( rect.width()/2, 0.0, -point-zTranslate );
            glRotatef( (1-frontDesktop)*360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0 );
            glTranslatef( 0.0, rect.height(), 0.0 );
            glCullFace( GL_FRONT );
            glCallList( glList + 2 );
            glTranslatef( 0.0, -rect.height(), 0.0 );
            glCullFace( GL_BACK );
            glCallList( glList + 2 );
            glPopMatrix();
            }
        glDisable( GL_CULL_FACE );

        if( effects->numScreens() > 1 && !slide && !bigCube  )
            {
            glPopMatrix();
            // revert change of projection matrix
            glMatrixMode( GL_PROJECTION );
            glPopMatrix();
            glMatrixMode( GL_MODELVIEW );
            }

        glDisable( GL_BLEND );
        glPopAttrib();

        // desktop name box - inspired from coverswitch
        if( displayDesktopName && (!slide) )
            {
            QColor color_frame;
            QColor color_text;
            color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
            color_frame.setAlphaF( 0.5 );
            color_text = KColorScheme( QPalette::Active, KColorScheme::Window ).foreground().color();
            if( start )
                {
                color_frame.setAlphaF( 0.5 * timeLine.value() );
                color_text.setAlphaF( timeLine.value() );
                }
            if( stop )
                {
                color_frame.setAlphaF( 0.5 - 0.5 * timeLine.value() );
                color_text.setAlphaF( 1.0 - timeLine.value() );
                }
            QFont text_font;
            text_font.setBold( true );
            text_font.setPointSize( 14 );
            glPushAttrib( GL_CURRENT_BIT );
            glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
            QRect frameRect = QRect( rect.width()*0.33f + rect.x(),
                rect.height() + rect.y() - QFontMetrics( text_font ).height() * 1.2f,
                rect.width()*0.33f,
                QFontMetrics( text_font ).height() * 1.2f );
            renderRoundBoxWithEdge( frameRect );
            effects->paintText( effects->desktopName( frontDesktop ),
                frameRect.center(),
                frameRect.width(),
                color_text,
                text_font );
            glPopAttrib();
            }
        if( effects->numScreens() > 1 && !slide && !bigCube  )
            {
            foreach( EffectWindow* w, windowsOnOtherScreens )
                {
                WindowPaintData wData( w );
                if( start && !w->isDesktop() && !w->isDock() )
                    wData.opacity *= (1.0 - timeLine.value());
                if( stop && !w->isDesktop() && !w->isDock() )
                    wData.opacity *= timeLine.value();
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        if( slide && dontSlidePanels )
            {
            foreach( EffectWindow* w, panels )
                {
                WindowPaintData wData( w );
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        if( slide && dontSlideStickyWindows )
            {
            foreach( EffectWindow* w, stickyWindows )
                {
                WindowPaintData wData( w );
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        }
    else
        {
        effects->paintScreen( mask, region, data );
        }
    }

void CubeEffect::rotateCube()
    {
    QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
    if( effects->numScreens() > 1 && bigCube )
        rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    float xScale = 1.0;
    float yScale = 1.0;
    if( effects->numScreens() > 1 && !bigCube  )
        {
        QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
        xScale = (float)rect.width()/(float)fullRect.width();
        yScale = (float)rect.height()/(float)fullRect.height();
        if( start )
            {
            xScale = xScale + (1.0 - xScale) * (1.0 - timeLine.value());
            yScale = yScale + (1.0 - yScale) * (1.0 - timeLine.value());
            if( rect.x() > 0 )
                glTranslatef( -rect.x()*(1.0 - timeLine.value()), 0.0, 0.0 );
            if( rect.y() > 0 )
                glTranslatef( 0.0, -rect.y()*(1.0 - timeLine.value()), 0.0 );
            }
        if( stop )
            {
            xScale = xScale + (1.0 - xScale) * timeLine.value();
            yScale = yScale + (1.0 - yScale) * timeLine.value();
            if( rect.x() > 0 )
                glTranslatef( -rect.x()*timeLine.value(), 0.0, 0.0 );
            if( rect.y() > 0 )
                glTranslatef( 0.0, -rect.y()*timeLine.value(), 0.0 );
            }
        glScalef( xScale, yScale, 1.0 );
        rect = fullRect;
        }

    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    float zTranslate = zPosition + zoom;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );
    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width()/2*tan(cubeAngle*0.5f*M_PI/180.0f);
    if( verticalRotating || verticalPosition != Normal || manualVerticalAngle != 0.0 )
        {
        // change the verticalPosition if manualVerticalAngle > 90 or < -90 degrees
        if( manualVerticalAngle <= -90.0 )
            {
            manualVerticalAngle += 90.0;
            if( verticalPosition == Normal )
                verticalPosition = Down;
            if( verticalPosition == Up )
                verticalPosition = Normal;
            }
        if( manualVerticalAngle >= 90.0 )
            {
            manualVerticalAngle -= 90.0;
            if( verticalPosition == Normal )
                verticalPosition = Up;
            if( verticalPosition == Down )
                verticalPosition = Normal;
            }
        float angle = 0.0;
        if( verticalPosition == Up )
            {
            angle = 90.0;
            if( !verticalRotating)
                {
                if( manualVerticalAngle < 0.0 )
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
                }
            }
        else if( verticalPosition == Down )
            {
            angle = -90.0;
            if( !verticalRotating)
                {
                if( manualVerticalAngle > 0.0 )
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
                }
            }
        else
            {
            angle = manualVerticalAngle;
            }
        if( verticalRotating )
            {
            angle *= verticalTimeLine.value();
            if( verticalPosition == Normal && verticalRotationDirection == Upwards )
                angle = -90.0 + 90*verticalTimeLine.value();
            if( verticalPosition == Normal && verticalRotationDirection == Downwards )
                angle = 90.0 - 90*verticalTimeLine.value();
            angle += manualVerticalAngle * (1.0-verticalTimeLine.value());
            }
        if( stop )
            angle *= (1.0 - timeLine.value());
        glTranslatef( rect.width()/2, rect.height()/2, -point-zTranslate );
        glRotatef( angle, 1.0, 0.0, 0.0 );
        glTranslatef( -rect.width()/2, -rect.height()/2, point+zTranslate );
        }
    if( rotating || (manualAngle != 0.0) )
        {
        int tempFrontDesktop = frontDesktop;
        if( manualAngle > internalCubeAngle * 0.5f )
            {
            manualAngle -= internalCubeAngle;
            tempFrontDesktop--;
            if( tempFrontDesktop == 0 )
                tempFrontDesktop = effects->numberOfDesktops();
            }
        if( manualAngle < -internalCubeAngle * 0.5f )
            {
            manualAngle += internalCubeAngle;
            tempFrontDesktop++;
            if( tempFrontDesktop > effects->numberOfDesktops() )
                tempFrontDesktop = 1;
            }
        float rotationAngle = internalCubeAngle * timeLine.value();
        if( rotationAngle > internalCubeAngle * 0.5f )
            {
            rotationAngle -= internalCubeAngle;
            if( !desktopChangedWhileRotating )
                {
                desktopChangedWhileRotating = true;
                if( rotationDirection == Left )
                    {
                    tempFrontDesktop++;
                    }
                else if( rotationDirection == Right )
                    {
                    tempFrontDesktop--;
                    }
                if( tempFrontDesktop > effects->numberOfDesktops() )
                    tempFrontDesktop = 1;
                else if( tempFrontDesktop == 0 )
                    tempFrontDesktop = effects->numberOfDesktops();
                }
            }
        // don't change front desktop during stop animation as this would break some logic
        if( !stop )
            frontDesktop = tempFrontDesktop;
        if( rotationDirection == Left )
            {
            rotationAngle *= -1;
            }
        if( stop )
            rotationAngle = manualAngle * (1.0 - timeLine.value());
        else
            rotationAngle += manualAngle * (1.0 - timeLine.value());
        glTranslatef( rect.width()/2, rect.height()/2, -point-zTranslate );
        glRotatef( rotationAngle, 0.0, 1.0, 0.0 );
        glTranslatef( -rect.width()/2, -rect.height()/2, point+zTranslate );
        }
    }

void CubeEffect::paintCube( int mask, QRegion region, ScreenPaintData& data )
    {
    QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
    if( effects->numScreens() > 1 && bigCube )
        rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    cube_painting = true;
    float zTranslate = zPosition + zoom;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );

    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width()/2*tan(cubeAngle*0.5f*M_PI/180.0f);

    for( int i=0; i<effects->numberOfDesktops(); i++ )
        {
        // start painting the cube
        painting_desktop = (i + frontDesktop )%effects->numberOfDesktops();
        if( painting_desktop == 0 )
            {
            painting_desktop = effects->numberOfDesktops();
            }
        ScreenPaintData newData = data;
        RotationData rot = RotationData();
        rot.axis = RotationData::YAxis;
        rot.angle = internalCubeAngle * i;
        rot.xRotationPoint = rect.width()/2;
        rot.zRotationPoint = -point;
        newData.rotation = &rot;
        newData.zTranslate = -zTranslate;
        effects->paintScreen( mask, region, newData );
        }
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
    }

void CubeEffect::paintSlideCube(int mask, QRegion region, KWin::ScreenPaintData& data)
    {
    // slide cube only paints to desktops at a time
    // first the horizontal rotations followed by vertical rotations
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    float point = rect.width()/2*tan(45.0f*M_PI/180.0f);
    cube_painting = true;
    painting_desktop = frontDesktop;

    ScreenPaintData firstFaceData = data;
    ScreenPaintData secondFaceData = data;
    RotationData firstFaceRot = RotationData();
    RotationData secondFaceRot = RotationData();
    RotationDirection direction = slideRotations.head();
    int secondDesktop;
    switch ( direction )
        {
        case Left:
            firstFaceRot.axis = RotationData::YAxis;
            secondFaceRot.axis = RotationData::YAxis;
            secondDesktop = effects->desktopToLeft( frontDesktop, true );
            firstFaceRot.angle = 90.0f*slideTimeLine.value();
            secondFaceRot.angle = -90.0f*(1.0f - slideTimeLine.value());
            break;
        case Right:
            firstFaceRot.axis = RotationData::YAxis;
            secondFaceRot.axis = RotationData::YAxis;
            secondDesktop = effects->desktopToRight( frontDesktop, true );
            firstFaceRot.angle = -90.0f*slideTimeLine.value();
            secondFaceRot.angle = 90.0f*(1.0f - slideTimeLine.value());
            break;
        case Upwards:
            firstFaceRot.axis = RotationData::XAxis;
            secondFaceRot.axis = RotationData::XAxis;
            secondDesktop = effects->desktopUp( frontDesktop, true);
            firstFaceRot.angle = -90.0f*slideTimeLine.value();
            secondFaceRot.angle = 90.0f*(1.0f - slideTimeLine.value());
            point = rect.height()/2*tan(45.0f*M_PI/180.0f);
            break;
        case Downwards:
            firstFaceRot.axis = RotationData::XAxis;
            secondFaceRot.axis = RotationData::XAxis;
            secondDesktop = effects->desktopDown( frontDesktop, true );
            firstFaceRot.angle = 90.0f*slideTimeLine.value();
            secondFaceRot.angle = -90.0f*(1.0f - slideTimeLine.value());
            point = rect.height()/2*tan(45.0f*M_PI/180.0f);
            break;
        default:
            // totally impossible
            return;
        }
    // front desktop
    firstFaceRot.xRotationPoint = rect.width()/2;
    firstFaceRot.yRotationPoint = rect.height()/2;
    firstFaceRot.zRotationPoint = -point;
    firstFaceData.rotation = &firstFaceRot;
    effects->paintScreen( mask, region, firstFaceData );
    // second desktop
    painting_desktop = secondDesktop;
    secondFaceRot.xRotationPoint = rect.width()/2;
    secondFaceRot.yRotationPoint = rect.height()/2;
    secondFaceRot.zRotationPoint = -point;
    secondFaceData.rotation = &secondFaceRot;
    effects->paintScreen( mask, region, secondFaceData );
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
    }


void CubeEffect::paintCap( float z, float zTexture )
    {
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop());
    if( ( !paintCaps ) || effects->numberOfDesktops() <= 2 )
        return;
    float opacity = cubeOpacity;
    if( start )
        opacity = 1.0 - (1.0 - opacity)*timeLine.value();
    if( stop )
        opacity = 1.0 - (1.0 - opacity)*( 1.0 - timeLine.value() );
    glColor4f( capColor.redF(), capColor.greenF(), capColor.blueF(), opacity );
    float angle = 360.0f/effects->numberOfDesktops();
    glPushMatrix();
    float zTranslate = zPosition + zoom;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );
    glTranslatef( rect.width()/2, 0.0, -z-zTranslate );
    glRotatef( (1-frontDesktop)*angle, 0.0, 1.0, 0.0 );

    if( !capListCreated )
        {
        capListCreated = true;
        glNewList( glList + 2, GL_COMPILE );
        if( texturedCaps && effects->numberOfDesktops() > 3 && capTexture )
            {
            paintCapStep( z, zTexture, true );
            }
        else
            paintCapStep( z, zTexture, false );
        glEndList();
        }
    else
        glCallList( glList + 2 );
    glPopMatrix();
    }

void CubeEffect::paintCapStep( float z, float zTexture, bool texture )
    {
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop());
    float angle = 360.0f/effects->numberOfDesktops();
    if( texture )
        capTexture->bind();
    for( int i=0; i<effects->numberOfDesktops(); i++ )
        {
        int triangleRows = effects->numberOfDesktops()*5;
        float zTriangleDistance = z/(float)triangleRows;
        float widthTriangle = tan( angle*0.5 * M_PI/180.0 ) * zTriangleDistance;
        float currentWidth = 0.0;
        glBegin( GL_TRIANGLES );
        float cosValue = cos( i*angle  * M_PI/180.0 );
        float sinValue = sin( i*angle  * M_PI/180.0 );
        for( int j=0; j<triangleRows; j++ )
            {
            float previousWidth = currentWidth;
            currentWidth = tan( angle*0.5 * M_PI/180.0 ) * zTriangleDistance * (j+1);
            int evenTriangles = 0;
            int oddTriangles = 0;
            for( int k=0; k<floor(currentWidth/widthTriangle*2-1+0.5f); k++ )
                {
                float x1 = -previousWidth;
                float x2 = -currentWidth;
                float x3 = 0.0;
                float z1 = 0.0;
                float z2 = 0.0;
                float z3 = 0.0;
                if( k%2 == 0 )
                    {
                    x1 += evenTriangles*widthTriangle*2;
                    x2 += evenTriangles*widthTriangle*2;
                    x3 = x2+widthTriangle*2;
                    z1 = j*zTriangleDistance;
                    z2 = (j+1)*zTriangleDistance;
                    z3 = (j+1)*zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    evenTriangles++;
                    }
                else
                    {
                    x1 += oddTriangles*widthTriangle*2;
                    x2 += (oddTriangles+1)*widthTriangle*2;
                    x3 = x1+widthTriangle*2;
                    z1 = j*zTriangleDistance;
                    z2 = (j+1)*zTriangleDistance;
                    z3 = j*zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    oddTriangles++;
                    }
                float texX1 = 0.0;
                float texX2 = 0.0;
                float texX3 = 0.0;
                float texY1 = 0.0;
                float texY2 = 0.0;
                float texY3 = 0.0;
                if( texture )
                    {
                    texX1 = x1/(rect.width())+0.5;
                    texY1 = 0.5 - z1/zTexture * 0.5;
                    texX2 = x2/(rect.width())+0.5;
                    texY2 = 0.5 - z2/zTexture * 0.5;
                    texX3 = x3/(rect.width())+0.5;
                    texY3 = 0.5 - z3/zTexture * 0.5;
                    glTexCoord2f( texX1, texY1 );
                    }
                glVertex3f( x1, 0.0, z1 );
                if( texture )
                    {
                    glTexCoord2f( texX2, texY2 );
                    }
                glVertex3f( x2, 0.0, z2 );
                if( texture )
                    {
                    glTexCoord2f( texX3, texY3 );
                    }
                glVertex3f( x3, 0.0, z3 );
                }
            }
        glEnd();
        }
    if( texture )
        {
        capTexture->unbind();
        }
    }

void CubeEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( activated )
        {
        if( start )
            {
            if( timeLine.value() == 1.0 )
                {
                start = false;
                timeLine.setProgress(0.0);
                // more rotations?
                if( !rotations.empty() )
                    {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        // more rotations follow -> linear curve
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::LinearCurve;
                            }
                        // last rotation step -> easeOut curve
                        else
                            {
                            currentShape = TimeLine::EaseOutCurve;
                            }
                        timeLine.setCurveShape( currentShape );
                        }
                    else
                        {
                        // if there is at least one more rotation, we can change to easeIn
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::EaseInCurve;
                            timeLine.setCurveShape( currentShape );
                            }
                        }
                    }
                }
            effects->addRepaintFull();
            return; // schedule_close could have been called, start has to finish first
            }
        if( stop )
            {
            if( timeLine.value() == 1.0 )
                {
                effects->setCurrentDesktop( frontDesktop );
                stop = false;
                timeLine.setProgress(0.0);
                activated = false;
                // set the new desktop
                if( keyboard_grab )
                    effects->ungrabKeyboard();
                keyboard_grab = false;
                effects->destroyInputWindow( input );
                windowsOnOtherScreens.clear();
                panels.clear();
                stickyWindows.clear();

                effects->setActiveFullScreenEffect( 0 );

                // delete the GL lists
                glDeleteLists( glList, 3 );
                }
            effects->addRepaintFull();
            }
        if( rotating || verticalRotating )
            {
            if( rotating && timeLine.value() == 1.0 )
                {
                timeLine.setProgress(0.0);
                rotating = false;
                desktopChangedWhileRotating = false;
                manualAngle = 0.0;
                // more rotations?
                if( !rotations.empty() )
                    {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        // more rotations follow -> linear curve
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::LinearCurve;
                            }
                        // last rotation step -> easeOut curve
                        else
                            {
                            currentShape = TimeLine::EaseOutCurve;
                            }
                        timeLine.setCurveShape( currentShape );
                        }
                    else
                        {
                        // if there is at least one more rotation, we can change to easeIn
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::EaseInCurve;
                            timeLine.setCurveShape( currentShape );
                            }
                        }
                    }
                else
                    {
                    // reset curve shape if there are no more rotations
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        currentShape = TimeLine::EaseInOutCurve;
                        timeLine.setCurveShape( currentShape );
                        }
                    }
                }
            if( verticalRotating && verticalTimeLine.value() == 1.0 )
                {
                verticalTimeLine.setProgress(0.0);
                verticalRotating = false;
                manualVerticalAngle = 0.0;
                // more rotations?
                if( !verticalRotations.empty() )
                    {
                    verticalRotationDirection = verticalRotations.dequeue();
                    verticalRotating = true;
                    if( verticalRotationDirection == Upwards )
                        {
                        if( verticalPosition == Normal )
                            verticalPosition = Up;
                        if( verticalPosition == Down )
                            verticalPosition = Normal;
                        }
                    if( verticalRotationDirection == Downwards )
                        {
                        if( verticalPosition == Normal )
                            verticalPosition = Down;
                        if( verticalPosition == Up )
                            verticalPosition = Normal;
                        }
                    }
                }
            effects->addRepaintFull();
            return; // rotation has to end before cube is closed
            }
        if( slide )
            {
            if( slideTimeLine.value() == 1.0 )
                {
                RotationDirection direction = slideRotations.dequeue();
                switch (direction)
                    {
                    case Left:
                        frontDesktop = effects->desktopToLeft( frontDesktop, true );
                        break;
                    case Right:
                        frontDesktop = effects->desktopToRight( frontDesktop, true );
                        break;
                    case Upwards:
                        frontDesktop = effects->desktopUp( frontDesktop, true );
                        break;
                    case Downwards:
                        frontDesktop = effects->desktopDown( frontDesktop, true );
                        break;
                    }
                slideTimeLine.setProgress( 0.0 );
                if( !slideRotations.empty() )
                    {
                    effects->addRepaintFull();
                    return;
                    }
                }
            else
                {
                effects->addRepaintFull();
                return;
                }
            }
        if( schedule_close )
            {
            schedule_close = false;
            if( !slide )
                {
                stop = true;
                }
            else
                {
                activated = false;
                slide = false;
                effects->setActiveFullScreenEffect( 0 );
                }
            effects->addRepaintFull();
            }
        }
    }

void CubeEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( activated )
        {
        if( cube_painting )
            {
            if( ( w->isDesktop() || w->isDock() ) && w->screen() != activeScreen && w->isOnDesktop( effects->currentDesktop() ) )
                {
                windowsOnOtherScreens.append( w );
                }
            if( (start || stop) && w->screen() != activeScreen && w->isOnDesktop( effects->currentDesktop() )
                && !w->isDesktop() && !w->isDock() )
                {
                windowsOnOtherScreens.append( w );
                }
            if( w->isOnDesktop( painting_desktop ))
                {
                QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );
                if( w->x() < rect.x() )
                    {
                    data.quads = data.quads.splitAtX( -w->x() );
                    }
                if( w->x() + w->width() > rect.x() + rect.width() )
                    {
                    data.quads = data.quads.splitAtX( rect.width() - w->x() );
                    }
                if( w->y() < rect.y() )
                    {
                    data.quads = data.quads.splitAtY( -w->y() );
                    }
                if( w->y() + w->height() > rect.y() + rect.height() )
                    {
                    data.quads = data.quads.splitAtY( rect.height() - w->y() );
                    }
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            else
                {
                // check for windows belonging to the previous desktop
                int prev_desktop = painting_desktop -1;
                if( prev_desktop == 0 )
                    prev_desktop = effects->numberOfDesktops();
                if( w->isOnDesktop( prev_desktop ) )
                    {
                    QRect rect = effects->clientArea( FullArea, activeScreen, prev_desktop);
                    if( w->x()+w->width() > rect.x() + rect.width() )
                        {
                        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                        data.quads = data.quads.splitAtX( rect.width() - w->x() );
                        if( w->y() < rect.y() )
                            {
                            data.quads = data.quads.splitAtY( -w->y() );
                            }
                        if( w->y() + w->height() > rect.y() + rect.height() )
                            {
                            data.quads = data.quads.splitAtY( rect.height() - w->y() );
                            }
                        data.setTransformed();
                        effects->prePaintWindow( w, data, time );
                        return;
                        }
                    }
                // check for windows belonging to the next desktop
                int next_desktop = painting_desktop +1;
                if( next_desktop > effects->numberOfDesktops() )
                    next_desktop = 1;
                if( w->isOnDesktop( next_desktop ) )
                    {
                    QRect rect = effects->clientArea( FullArea, activeScreen, next_desktop);
                    if( w->x() < rect.x() )
                        {
                        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                        data.quads = data.quads.splitAtX( -w->x() );
                        if( w->y() < rect.y() )
                            {
                            data.quads = data.quads.splitAtY( -w->y() );
                            }
                        if( w->y() + w->height() > rect.y() + rect.height() )
                            {
                            data.quads = data.quads.splitAtY( rect.height() - w->y() );
                            }
                        data.setTransformed();
                        effects->prePaintWindow( w, data, time );
                        return;
                        }
                    }
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            if( slide && dontSlidePanels && w->isDock())
                {
                panels.insert( w );
                }
            if( slide && dontSlideStickyWindows && !w->isDock() &&
                !w->isDesktop() && w->isOnAllDesktops())
                {
                stickyWindows.insert( w );
                }
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void CubeEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( activated && cube_painting )
        {
        if( slide && dontSlidePanels && w->isDock() )
            return;
        if( slide && dontSlideStickyWindows &&
            w->isOnAllDesktops() && !w->isDock() && !w->isDesktop() )
            return;
        //kDebug(1212) << w->caption();
        float opacity = cubeOpacity;
        if( slide )
            opacity = 1.0f;
        if( start )
            {
            opacity = 1.0 - (1.0 - opacity)*timeLine.value();
            if( reflectionPainting )
                opacity = 0.5 + ( cubeOpacity - 0.5 )*timeLine.value();
            // fade in windows belonging to different desktops
            if( painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
                opacity = timeLine.value() * cubeOpacity;
            }
        if( stop )
            {
            opacity = 1.0 - (1.0 - opacity)*( 1.0 - timeLine.value() );
            if( reflectionPainting )
                opacity = 0.5 + ( cubeOpacity - 0.5 )*( 1.0 - timeLine.value() );
            // fade out windows belonging to different desktops
            if( painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
                opacity = cubeOpacity * (1.0 - timeLine.value());
            }
        bool slideOpacity = false;
        // fade in windows from different desktops in first slide step
        if( slide && painting_desktop == oldDesktop && (!w->isOnDesktop( painting_desktop )) )
            {
            opacity = timeLine.value();
            slideOpacity = true;
            }
        // fade out windows from different desktops in last slide step
        if( slide && rotations.empty() && painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
            {
            opacity = 1.0 - timeLine.value();
            slideOpacity = true;
            }
        // check for windows belonging to the previous desktop
        int prev_desktop = painting_desktop -1;
        if( prev_desktop == 0 )
            prev_desktop = effects->numberOfDesktops();
        int next_desktop = painting_desktop +1;
        if( next_desktop > effects->numberOfDesktops() )
            next_desktop = 1;
        if( w->isOnDesktop( prev_desktop ) && ( mask & PAINT_WINDOW_TRANSFORMED ) )
            {
            QRect rect = effects->clientArea( FullArea, activeScreen, prev_desktop);
            data.xTranslate = -rect.width();
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( quad.right() > rect.width() - w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->isOnDesktop( next_desktop ) && ( mask & PAINT_WINDOW_TRANSFORMED ) )
            {
            QRect rect = effects->clientArea( FullArea, activeScreen, next_desktop);
            data.xTranslate = rect.width();
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( w->x() + quad.right() <= rect.x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );

        if( start || stop )
            {
            // we have to change opacity values for fade in/out of windows which are shown on front-desktop
            if( prev_desktop == effects->currentDesktop() && w->x() < rect.x() )
                {
                if( start )
                    opacity = timeLine.value() * cubeOpacity;
                if( stop )
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            if( next_desktop == effects->currentDesktop() && w->x() + w->width() > rect.x() + rect.width() )
                {
                if( start )
                    opacity = timeLine.value() * cubeOpacity;
                if( stop )
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            }
        if( !slide || (slide && !w->isDesktop()) || slideOpacity )
            {
            // HACK set opacity to 0.99 in case of fully opaque to ensure that windows are painted in correct sequence
            // bug #173214
            if( opacity > 0.99f )
                opacity = 0.99f;
            if( opacityDesktopOnly && !w->isDesktop() )
                opacity = 0.99f;
            data.opacity *= opacity;
            }

        if( w->isOnDesktop(painting_desktop) && w->x() < rect.x() )
            {
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( quad.right() > -w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->isOnDesktop(painting_desktop) && w->x() + w->width() > rect.x() + rect.width() )
            {
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( quad.right() <= rect.width() - w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->y() < rect.y() )
            {
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( quad.bottom() > -w->y() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->y() + w->height() > rect.y() + rect.height() )
            {
            WindowQuadList new_quads;
            foreach( const WindowQuad &quad, data.quads )
                {
                if( quad.bottom() <= rect.height() - w->y() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->isDesktop() && effects->numScreens() > 1 && paintCaps && !slide )
            {
            QRegion paint = QRegion( rect );
            for( int i=0; i<effects->numScreens(); i++ )
                {
                if( i == w->screen() )
                    continue;
                paint = paint.subtracted( QRegion( effects->clientArea( ScreenArea, i, painting_desktop )));
                }
            paint = paint.subtracted( QRegion( w->geometry()));
            // in case of free area in multiscreen setup fill it with cap color
            if( !paint.isEmpty() )
                {
                glColor4f( capColor.redF(), capColor.greenF(), capColor.blueF(), cubeOpacity );
                glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
                glEnable( GL_BLEND );
                glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
                glBegin( GL_QUADS );
                foreach( QRect paintRect, paint.rects() )
                    {
                    glVertex2f( paintRect.x(), paintRect.y() );
                    glVertex2f( paintRect.x()+paintRect.width(), paintRect.y() );
                    glVertex2f( paintRect.x()+paintRect.width(), paintRect.y() + paintRect.height() );
                    glVertex2f( paintRect.x(), paintRect.y() + paintRect.height() );
                    }
                glEnd();
                glDisable( GL_BLEND );
                glPopAttrib();
                }
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

bool CubeEffect::borderActivated( ElectricBorder border )
    {
    if( border != borderActivate )
        return false;
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return true;
    kDebug(1212) << "border activated";
    toggle();
    return true;
    }

void CubeEffect::toggle()
    {
    if( ( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this ) ||
        effects->numberOfDesktops() < 2 )
        return;
    if( !activated )
        {
        setActive( true );
        }
    else
        {
        setActive( false );
        }
    }

void CubeEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    if( stop )
        return;
    // taken from desktopgrid.cpp
    if( e->type() == QEvent::KeyPress )
        {
        int desktop = -1;
        // switch by F<number> or just <number>
        if( e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35 )
            desktop = e->key() - Qt::Key_F1 + 1;
        else if( e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9 )
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if( desktop != -1 )
            {
            if( desktop <= effects->numberOfDesktops())
                {
                // we have to rotate to chosen desktop
                // and end effect when rotation finished
                rotateToDesktop( desktop );
                setActive( false );
                }
            return;
            }
        switch( e->key())
            { // wrap only on autorepeat
            case Qt::Key_Left:
                // rotate to previous desktop
                kDebug(1212) << "left";
                if( !rotating && !start )
                    {
                    rotating = true;
                    if( invertKeys )
                        rotationDirection = Right;
                    else
                        rotationDirection = Left;
                    }
                else
                    {
                    if( rotations.count() < effects->numberOfDesktops() )
                        {
                        if( invertKeys )
                            rotations.enqueue( Right );
                        else
                            rotations.enqueue( Left );
                        }
                    }
                break;
            case Qt::Key_Right:
                // rotate to next desktop
                kDebug(1212) << "right";
                if( !rotating && !start )
                    {
                    rotating = true;
                    if( invertKeys )
                        rotationDirection = Left;
                    else
                        rotationDirection = Right;
                    }
                else
                    {
                    if( rotations.count() < effects->numberOfDesktops() )
                        {
                        if( invertKeys )
                            rotations.enqueue( Left );
                        else
                            rotations.enqueue( Right );
                        }
                    }
                break;
            case Qt::Key_Up:
                kDebug(1212) << "up";
                if( invertKeys )
                    {
                    if( verticalPosition != Down )
                        {
                        if( !verticalRotating )
                            {
                            verticalRotating = true;
                            verticalRotationDirection = Downwards;
                            if( verticalPosition == Normal )
                                verticalPosition = Down;
                            if( verticalPosition == Up )
                                verticalPosition = Normal;
                            }
                        else
                            {
                            verticalRotations.enqueue( Downwards );
                            }
                        }
                    else if( manualVerticalAngle > 0.0 && !verticalRotating )
                        {
                        // rotate to down position from the manual position
                        verticalRotating = true;
                        verticalRotationDirection = Downwards;
                        verticalPosition = Down;
                        manualVerticalAngle -= 90.0;
                        }
                    }
                else
                    {
                    if( verticalPosition != Up )
                        {
                        if( !verticalRotating )
                            {
                            verticalRotating = true;
                            verticalRotationDirection = Upwards;
                            if( verticalPosition == Normal )
                                verticalPosition = Up;
                            if( verticalPosition == Down )
                                verticalPosition = Normal;
                            }
                        else
                            {
                            verticalRotations.enqueue( Upwards );
                            }
                        }
                    else if( manualVerticalAngle < 0.0 && !verticalRotating )
                        {
                        // rotate to up position from the manual position
                        verticalRotating = true;
                        verticalRotationDirection = Upwards;
                        verticalPosition = Up;
                        manualVerticalAngle += 90.0;
                        }
                    }
                break;
            case Qt::Key_Down:
                kDebug(1212) << "down";
                if( invertKeys )
                    {
                    if( verticalPosition != Up )
                        {
                        if( !verticalRotating )
                            {
                            verticalRotating = true;
                            verticalRotationDirection = Upwards;
                            if( verticalPosition == Normal )
                                verticalPosition = Up;
                            if( verticalPosition == Down )
                                verticalPosition = Normal;
                            }
                        else
                            {
                            verticalRotations.enqueue( Upwards );
                            }
                        }
                    else if( manualVerticalAngle < 0.0 && !verticalRotating )
                        {
                        // rotate to up position from the manual position
                        verticalRotating = true;
                        verticalRotationDirection = Upwards;
                        verticalPosition = Up;
                        manualVerticalAngle += 90.0;
                        }
                    }
                else
                    {
                    if( verticalPosition != Down )
                        {
                        if( !verticalRotating )
                            {
                            verticalRotating = true;
                            verticalRotationDirection = Downwards;
                            if( verticalPosition == Normal )
                                verticalPosition = Down;
                            if( verticalPosition == Up )
                                verticalPosition = Normal;
                            }
                        else
                            {
                            verticalRotations.enqueue( Downwards );
                            }
                        }
                    else if( manualVerticalAngle > 0.0 && !verticalRotating )
                        {
                        // rotate to down position from the manual position
                        verticalRotating = true;
                        verticalRotationDirection = Downwards;
                        verticalPosition = Down;
                        manualVerticalAngle -= 90.0;
                        }
                    }
                break;
            case Qt::Key_Escape:
                rotateToDesktop( effects->currentDesktop() );
                setActive( false );
                return;
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Space:
                setActive( false );
                return;
            case Qt::Key_Plus:
                zoom -= 10.0;
                zoom = qMax( -zPosition, zoom );
                break;
            case Qt::Key_Minus:
                zoom += 10.0f;
                break;
            default:
                break;
            }
        effects->addRepaintFull();
        }
    }

void CubeEffect::rotateToDesktop( int desktop )
    {
    int tempFrontDesktop = frontDesktop;
    if( !rotations.empty() )
        {
        // all scheduled rotations will be removed as a speed up
        rotations.clear();
        }
    if( rotating && !desktopChangedWhileRotating )
        {
        // front desktop will change during the actual rotation - this has to be considered
        if( rotationDirection == Left )
            {
            tempFrontDesktop++;
            }
        else if( rotationDirection == Right )
            {
            tempFrontDesktop--;
            }
        if( tempFrontDesktop > effects->numberOfDesktops() )
            tempFrontDesktop = 1;
        else if( tempFrontDesktop == 0 )
            tempFrontDesktop = effects->numberOfDesktops();
        }
    // find the fastest rotation path from tempFrontDesktop to desktop
    int rightRotations = tempFrontDesktop - desktop;
    if( rightRotations < 0 )
        rightRotations += effects->numberOfDesktops();
    int leftRotations = desktop - tempFrontDesktop;
    if( leftRotations < 0 )
        leftRotations += effects->numberOfDesktops();
    if( leftRotations <= rightRotations )
        {
        for( int i=0; i<leftRotations; i++ )
            {
            rotations.enqueue( Left );
            }
        }
    else
        {
        for( int i=0; i<rightRotations; i++ )
            {
            rotations.enqueue( Right );
            }
        }
    if( !start && !rotating && !rotations.empty() )
        {
        rotating = true;
        rotationDirection = rotations.dequeue();
        }
    // change timeline curve if more rotations are following
    if( !rotations.empty() )
        {
        currentShape = TimeLine::EaseInCurve;
        timeLine.setCurveShape( currentShape );
        }
    }

void CubeEffect::setActive( bool active )
    {
    if( active )
        {
        effects->startMousePolling();
        activated = true;
        activeScreen = effects->activeScreen();
        if( !slide )
            {
            keyboard_grab = effects->grabKeyboard( this );
            input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(),
                Qt::OpenHandCursor );
            frontDesktop = effects->currentDesktop();
            zoom = 0.0;
            start = true;
            }
        effects->setActiveFullScreenEffect( this );
        kDebug(1212) << "Cube is activated";
        verticalPosition = Normal;
        verticalRotating = false;
        manualAngle = 0.0;
        manualVerticalAngle = 0.0;
        if( reflection && !slide )
            {
            // clip parts above the reflection area
            double eqn[4] = {0.0, 1.0, 0.0, 0.0};
            glPushMatrix();
            QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
            if( effects->numScreens() > 1 && bigCube )
                rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
            glTranslatef( 0.0, rect.height(), 0.0 );
            glClipPlane( GL_CLIP_PLANE0, eqn );
            glPopMatrix();
            }
        // create the needed GL lists
        glList = glGenLists(3);
        capListCreated = false;
        recompileList = true;
        // create the capList
        if( paintCaps && !slide )
            {
            QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
            if( effects->numScreens() > 1 && (slide || bigCube ) )
                rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
            float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f);
            float point = rect.width()/2*tan(cubeAngle*0.5f*M_PI/180.0f);
            float zTexture = rect.width()/2*tan(45.0f*M_PI/180.0f);
            paintCap( point, zTexture );
            }
        
        effects->addRepaintFull();
        }
    else
        {
        effects->stopMousePolling();
        schedule_close = true;
        // we have to add a repaint, to start the deactivating
        effects->addRepaintFull();
        }
    }

void CubeEffect::mouseChanged( const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, 
            Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers, Qt::KeyboardModifiers )
    {
    if( !activated )
        return;
    if( tabBoxMode )
        return;
    if( stop || slide )
        return;
    QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
    if( effects->numScreens() > 1 && (slide || bigCube ) )
        rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    if( buttons.testFlag( Qt::LeftButton ) )
        {
        bool repaint = false;
        // vertical movement only if there is not a rotation
        if( !verticalRotating )
            {
            // display height corresponds to 180*
            int deltaY = pos.y() - oldpos.y();
            float deltaVerticalDegrees = (float)deltaY/rect.height()*180.0f;
            if( invertMouse )
                manualVerticalAngle += deltaVerticalDegrees;
            else
                manualVerticalAngle -= deltaVerticalDegrees;
            if( deltaVerticalDegrees != 0.0 )
                repaint = true;
            }
        // horizontal movement only if there is not a rotation
        if( !rotating )
            {
            // display width corresponds to sum of angles of the polyhedron
            int deltaX = oldpos.x() - pos.x();
            float deltaDegrees = (float)deltaX/rect.width() * 360.0f;
            if( invertMouse )
                manualAngle += deltaDegrees;
            else
                manualAngle -= deltaDegrees;
            if( deltaDegrees != 0.0 )
                repaint = true;
            }
        if( repaint )
            {
            recompileList = true;
            effects->addRepaintFull();
            }
        }
    if( !oldbuttons.testFlag( Qt::LeftButton ) && buttons.testFlag( Qt::LeftButton ) )
        {
        XDefineCursor( display(), input, QCursor( Qt::ClosedHandCursor).handle() );
        }
    if( oldbuttons.testFlag( Qt::LeftButton) && !buttons.testFlag( Qt::LeftButton ) )
        {
        XDefineCursor( display(), input, QCursor( Qt::OpenHandCursor).handle() );
        if( closeOnMouseRelease )
            setActive( false );
        }
    if( oldbuttons.testFlag( Qt::RightButton) && !buttons.testFlag( Qt::RightButton ) )
        {
        // end effect on right mouse button
        setActive( false );
        }
    }

void CubeEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == input );
    QMouseEvent *mouse = dynamic_cast< QMouseEvent* >( e );
    if( mouse && mouse->type() == QEvent::MouseButtonRelease )
        {
        if( mouse->button() == Qt::XButton1 )
            {
            if( !rotating && !start )
                {
                rotating = true;
                if( invertMouse )
                    rotationDirection = Right;
                else
                    rotationDirection = Left;
                }
            else
                {
                if( rotations.count() < effects->numberOfDesktops() )
                    {
                    if( invertMouse )
                        rotations.enqueue( Right );
                    else
                        rotations.enqueue( Left );
                    }
                }
            effects->addRepaintFull();
            }
        if( mouse->button() == Qt::XButton2 )
            {
            if( !rotating && !start )
                {
                rotating = true;
                if( invertMouse )
                    rotationDirection = Left;
                else
                    rotationDirection = Right;
                }
            else
                {
                if( rotations.count() < effects->numberOfDesktops() )
                    {
                    if( invertMouse )
                        rotations.enqueue( Left );
                    else
                        rotations.enqueue( Right );
                    }
                }
            effects->addRepaintFull();
            }
        }
    }

void CubeEffect::desktopChanged( int old )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( activated && !slide )
        return;
    if( !animateDesktopChange )
        return;
    slide = true;
    bool activate = true;
    if( !slideRotations.empty() )
        {
        // last slide still in progress
        activate = false;
        old = frontDesktop;
        RotationDirection direction = slideRotations.dequeue();
        slideRotations.clear();
        slideRotations.enqueue( direction );
        }
    oldDesktop = old;
    // calculate distance in respect to pager
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    int x_distance = (( effects->currentDesktop() - 1 ) % x ) - (( old - 1 ) % x );
    if( qAbs( x_distance ) > x/2 )
        {
        int sign = -1 * (x_distance/qAbs( x_distance ));
        x_distance = sign * ( x - qAbs( x_distance ));
        }
    if( x_distance > 0 )
        {
        for( int i=0; i<x_distance; i++ )
            {
            slideRotations.enqueue( Right );
            }
        }
    else if( x_distance < 0 )
        {
        x_distance *= -1;
        for( int i=0; i<x_distance; i++ )
            {
            slideRotations.enqueue( Left );
            }
        }
    int y_distance = (( effects->currentDesktop() -1 ) / x ) - (( old - 1 ) / x );
    if( qAbs( y_distance ) > y/2 )
        {
        int sign = -1 * (y_distance/qAbs( y_distance ));
        y_distance = sign * ( y - qAbs( y_distance ));
        }
    if( y_distance > 0 )
        {
        for( int i=0; i<y_distance; i++ )
            {
            slideRotations.enqueue( Downwards );
            }
        }
    if( y_distance < 0 )
        {
        y_distance *= -1;
        for( int i=0; i<y_distance; i++ )
            {
            slideRotations.enqueue( Upwards );
            }
        }
    if( activate )
        {
        setActive( true );
        slideTimeLine.setProgress( 0.0 );
        frontDesktop = old;
        setActive( false );
        }
    }


void CubeEffect::tabBoxAdded( int mode )
    {
    if( activated )
        return;
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( useForTabBox && mode != TabBoxWindowsMode )
        {
        effects->refTabBox();
        tabBoxMode = true;
        setActive( true );
        rotateToDesktop( effects->currentTabBoxDesktop() );
        }
    }

void CubeEffect::tabBoxUpdated()
    {
    if( activated )
        rotateToDesktop( effects->currentTabBoxDesktop() );
    }

void CubeEffect::tabBoxClosed()
    {
    if( activated )
        {
        effects->unrefTabBox();
        tabBoxMode = false;
        setActive( false );
        }
    }

} // namespace
