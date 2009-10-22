/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

Based on Compiz Fusion cube gear plugin by Dennis Kasprzyk:
    http://gitweb.compiz-fusion.org/?p=fusion/plugins/gears;a=blob;f=gears.c;hb=HEAD
Which is based on glxgears.c by Brian Paul:
    http://cvsweb.xfree86.org/cvsweb/xc/programs/glxgears/glxgears.c
*********************************************************************/

#include "gears.h"
#include "../cube/cube_proxy.h"

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( gears, GearsEffect )
KWIN_EFFECT_SUPPORTED( gears, GearsEffect::supported() )

GearsEffect::GearsEffect()
    : CubeInsideEffect()
    , m_active( false )
    , m_contentRotation( 0.0f )
    , m_angle( 0.0f )
    {
    CubeEffectProxy* proxy =
        static_cast<CubeEffectProxy*>( effects->getProxy( "cube" ) );
    if( proxy )
        proxy->registerCubeInsideEffect( this );
    }

GearsEffect::~GearsEffect()
    {
    CubeEffectProxy* proxy =
    static_cast<CubeEffectProxy*>( effects->getProxy( "cube" ) );
    if( proxy )
        proxy->unregisterCubeInsideEffect( this );
    }

bool GearsEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void GearsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( m_active )
        {
        m_contentRotation += time * 360.0f / 20000.0f;
        if( m_contentRotation > 360.0f )
            m_contentRotation -= 360.0f;
        m_angle += time * 360.0f / 8000.0f;
        if( m_angle > 360.0f )
            m_angle -= 360.0f;
        }
    effects->prePaintScreen( data, time );
    }

void GearsEffect::postPaintScreen()
    {
    if( m_active )
        {
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }


void GearsEffect::paint()
    {
    if( m_active )
        {
        paintGears();
        }
    }

void GearsEffect::setActive( bool active )
    {
    m_active = active;
    if( active )
        initGears();
    else
        endGears();
    }

void GearsEffect::gear( float inner_radius, float outer_radius, float width, int teeth, float tooth_depth )
    {
    GLint i;
    GLfloat r0, r1, r2, maxr2, minr2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    maxr2 = r2 = outer_radius + tooth_depth / 2.0;
    minr2 = r2;

    da = 2.0 * M_PI / teeth / 4.0;

    glShadeModel( GL_FLAT );

    glNormal3f( 0.0, 0.0, 1.0 );

    /* draw front face */
    glBegin( GL_QUAD_STRIP );

    for( i = 0; i <= teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );

        if( i < teeth )
            {
            glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
            glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
            }
        }

    glEnd();

    /* draw front sides of teeth */
    glBegin( GL_QUADS );

    for( i = 0; i < teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), width * 0.5 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
        r2 = minr2;
        }

    r2 = maxr2;

    glEnd();

    glNormal3f( 0.0, 0.0, -1.0 );

    /* draw back face */
    glBegin( GL_QUAD_STRIP );

    for( i = 0; i <= teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), -width * 0.5 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );

        if( i < teeth )
            {
            glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
            glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );
            }
        }

    glEnd();

    /* draw back sides of teeth */
    glBegin( GL_QUADS );
    da = 2.0 * M_PI / teeth / 4.0;

    for( i = 0; i < teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), -width * 0.5 );
        glVertex3f( r2 * cos (angle + da), r2 * sin (angle + da), -width * 0.5 );
        glVertex3f( r1 * cos (angle), r1 * sin (angle), -width * 0.5 );
        r2 = minr2;
        }

    r2 = maxr2;

    glEnd();

    /* draw outward faces of teeth */
    glBegin( GL_QUAD_STRIP );

    for( i = 0; i < teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), -width * 0.5 );
        u = r2 * cos( angle + da ) - r1 * cos( angle );
        v = r2 * sin( angle + da ) - r1 * sin( angle );
        len = sqrt( u * u + v * v );
        u /= len;
        v /= len;
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), -width * 0.5 );
        glNormal3f( cos( angle + 1.5 * da ), sin( angle + 1.5 * da ), 0.0 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), -width * 0.5 );
        u = r1 * cos( angle + 3 * da ) - r2 * cos( angle + 2 * da );
        v = r1 * sin( angle + 3 * da ) - r2 * sin( angle + 2 * da );
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
        glNormal3f( cos( angle + 3.5 * da ), sin( angle + 3.5 * da ), 0.0 );
        r2 = minr2;
        }

    r2 = maxr2;

    glVertex3f( r1 * cos( 0 ), r1 * sin( 0 ), width * 0.5 );
    glVertex3f( r1 * cos( 0 ), r1 * sin( 0 ), -width * 0.5 );

    glEnd();

    glShadeModel( GL_SMOOTH );

    /* draw inside radius cylinder */
    glBegin( GL_QUAD_STRIP );

    for( i = 0; i <= teeth; i++ )
        {
        angle = i * 2.0 * M_PI / teeth;
        glNormal3f( -cos( angle ), -sin( angle ), 0.0 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
        }
    glEnd();
    }

void GearsEffect::paintGears()
    {
    static GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0 };
    QRect rect = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop() );

    glPushMatrix();
    // invert scale
    float fovy = 60.0f;
    float zNear = 0.1f;
    float ymax = zNear * tan( fovy  * M_PI / 360.0f );
    float ymin = -ymax;
    float xmin =  ymin;
    float xmax = ymax;
    float scaleFactor = 1.1 * tan( fovy * M_PI / 360.0f )/ymax;
    glScalef( 1.0f/((xmax-xmin)*scaleFactor/displayWidth()), 1.0f/(-(ymax-ymin)*scaleFactor/displayHeight()), 1000.0f );

    glPushAttrib( GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_LIGHTING_BIT );

    glDisable( GL_BLEND );

    glPushMatrix();

    // content rotation requires depth test - so disabled
//     glRotatef( m_contentRotation, 0.0, 1.0, 0.0 );

    glScalef( 0.05, 0.05, 0.05 );

    glEnable( GL_NORMALIZE );
    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT1 );
    glDisable( GL_COLOR_MATERIAL );

    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

    glPushMatrix();
    glTranslatef( -3.0, -2.0, 0.0 );
    glRotatef( m_angle, 0.0, 0.0, 1.0 );
    glCallList( m_gear1 );
    glPopMatrix();

    glPushMatrix();
    glTranslatef( 3.1, -2.0, 0.0 );
    glRotatef( -2.0 * m_angle - 9.0, 0.0, 0.0, 1.0 );
    glCallList( m_gear2 );
    glPopMatrix();

    glPushMatrix();
    glTranslatef( -3.1, 4.2, 0.0 );
    glRotatef( -2.0 * m_angle - 25.0, 0.0, 0.0, 1.0 );
    glCallList( m_gear3 );
    glPopMatrix();

    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white );

    glPopMatrix();

    glDisable( GL_LIGHT1 );
    glDisable( GL_NORMALIZE );
    glEnable( GL_COLOR_MATERIAL );

    glDisable( GL_LIGHTING );

    glPopMatrix();
    glPopAttrib();
    }

void GearsEffect::initGears()
    {
    static GLfloat pos[4]         = { 5.0f, 5.0f, 10.0f, 0.0f };
    static GLfloat red[4]         = { 0.8f, 0.1f, 0.0f, 1.0f };
    static GLfloat green[4]       = { 0.0f, 0.8f, 0.2f, 1.0f };
    static GLfloat blue[4]        = { 0.2f, 0.2f, 1.0f, 1.0f };
    static GLfloat ambientLight[] = { 0.3f, 0.3f, 0.3f, 0.3f };
    static GLfloat diffuseLight[] = { 0.5f, 0.5f, 0.5f, 0.5f };

    glLightfv( GL_LIGHT1, GL_AMBIENT, ambientLight );
    glLightfv( GL_LIGHT1, GL_DIFFUSE, diffuseLight );
    glLightfv( GL_LIGHT1, GL_POSITION, pos );

    m_gear1 = glGenLists( 1 );
    glNewList( m_gear1, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red );
    gear( 1.0f, 4.0f, 1.0f, 20, 0.7f );
    glEndList();

    m_gear2 = glGenLists( 1 );
    glNewList( m_gear2, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green );
    gear( 0.5f, 2.0f, 2.0f, 10, 0.7f );
    glEndList();

    m_gear3 = glGenLists( 1 );
    glNewList( m_gear3, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue );
    gear( 1.3f, 2.0f, 0.5f, 10, 0.7f );
    glEndList();
    }

void GearsEffect::endGears()
    {
    glDeleteLists( m_gear1, 1 );
    glDeleteLists( m_gear2, 1 );
    glDeleteLists( m_gear3, 1 );
    }

} // namespace
