/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

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

#include "kwinglutils.h"

#ifdef KWIN_HAVE_OPENGL

#include "kwinglobals.h"
#include "kwineffects.h"

#include "kdebug.h"
#include <kstandarddirs.h>

#include <QPixmap>
#include <QImage>
#include <QHash>
#include <QFile>



#define MAKE_GL_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )


namespace KWin
{
// Variables
// GL version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glVersion;
// GLX version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glXVersion;
// List of all supported GL and GLX extensions
static QStringList glExtensions;
static QStringList glxExtensions;

int glTextureUnitsCount;


// Functions
void initGLX()
    {
    // Get GLX version
    int major, minor;
    glXQueryVersion( display(), &major, &minor );
    glXVersion = MAKE_GL_VERSION( major, minor, 0 );
    // Get list of supported GLX extensions
    glxExtensions = QString((const char*)glXQueryExtensionsString(
        display(), DefaultScreen( display()))).split(" ");

    glxResolveFunctions();
    }

void initGL()
    {
    // Get OpenGL version
    QString glversionstring = QString((const char*)glGetString(GL_VERSION));
    QStringList glversioninfo = glversionstring.left(glversionstring.indexOf(' ')).split('.');
    glVersion = MAKE_GL_VERSION(glversioninfo[0].toInt(), glversioninfo[1].toInt(),
                                    glversioninfo.count() > 2 ? glversioninfo[2].toInt() : 0);
    // Get list of supported OpenGL extensions
    glExtensions = QString((const char*)glGetString(GL_EXTENSIONS)).split(" ");

    // handle OpenGL extensions functions
    glResolveFunctions();

    GLTexture::initStatic();
    GLShader::initStatic();
    GLRenderTarget::initStatic();
    }

bool hasGLVersion(int major, int minor, int release)
    {
    return glVersion >= MAKE_GL_VERSION(major, minor, release);
    }

bool hasGLXVersion(int major, int minor, int release)
    {
    return glXVersion >= MAKE_GL_VERSION(major, minor, release);
    }

bool hasGLExtension(const QString& extension)
    {
    return glExtensions.contains(extension) || glxExtensions.contains(extension);
    }

bool checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        {
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) ;
        return true;
        }
    return false;
    }

int nearestPowerOfTwo( int x )
    {
    // This method had been copied from Qt's nearest_gl_texture_size()
    int n = 0, last = 0;
    for (int s = 0; s < 32; ++s) {
        if (((x>>s) & 1) == 1) {
            ++n;
            last = s;
        }
    }
    if (n > 1)
        return 1 << (last+1);
    return 1 << last;
    }

void renderGLGeometry( int count, const float* vertices, const float* texture, const float* color,
    int dim, int stride )
    {
    return renderGLGeometry( false, QRegion(), count, vertices, texture, color, dim, stride );
    }

void renderGLGeometry( int mask, const QRegion& region, int count,
    const float* vertices, const float* texture, const float* color,
    int dim, int stride )
    {
    return renderGLGeometry( !( mask & ( Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_SCREEN_TRANSFORMED )),
        region, count, vertices, texture, color, dim, stride );
    }

void renderGLGeometry( bool clip, const QRegion& region, int count,
    const float* vertices, const float* texture, const float* color,
    int dim, int stride )
    {
    // Using arrays only makes sense if we have larger number of vertices.
    //  Otherwise overhead of enabling/disabling them is too big.
    bool use_arrays = (count > 5);

    if( use_arrays )
        {
        glPushAttrib( GL_ENABLE_BIT );
        glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );
        // Enable arrays
        glEnableClientState( GL_VERTEX_ARRAY );
        glVertexPointer( dim, GL_FLOAT, stride, vertices );
        if( texture != NULL )
            {
            glEnableClientState( GL_TEXTURE_COORD_ARRAY );
            glTexCoordPointer( 2, GL_FLOAT, stride, texture );
            }
        if( color != NULL )
            {
            glEnableClientState( GL_COLOR_ARRAY );
            glColorPointer( 4, GL_FLOAT, stride, color );
            }
        }

    // Render
    if( !clip )
        {
        // Just draw the entire window, no clipping
        if( use_arrays )
            glDrawArrays( GL_QUADS, 0, count );
        else
            renderGLGeometryImmediate( count, vertices, texture, color, dim, stride );
        }
    else
        {
        // Make sure there's only a single quad (no transformed vertices)
        // Clip using scissoring
        glEnable( GL_SCISSOR_TEST );
        int dh = displayHeight();
        if( use_arrays )
            {
            foreach( QRect r, region.rects())
                {
                // Scissor rect has to be given in OpenGL coords
                glScissor(r.x(), dh - r.y() - r.height(), r.width(), r.height());
                glDrawArrays( GL_QUADS, 0, count );
                }
            }
        else
            {
            foreach( QRect r, region.rects())
                {
                // Scissor rect has to be given in OpenGL coords
                glScissor(r.x(), dh - r.y() - r.height(), r.width(), r.height());
                renderGLGeometryImmediate( count, vertices, texture, color, dim, stride );
                }
            }
        }

    if( use_arrays )
        {
        glPopClientAttrib();
        glPopAttrib();
        }
    }

void renderGLGeometryImmediate( int count, const float* vertices, const float* texture, const float* color,
      int dim, int stride )
{
    // Find out correct glVertex*fv function according to dim parameter.
    void ( *glVertexFunc )( const float* ) = glVertex2fv;
    if( dim == 3 )
        glVertexFunc = glVertex3fv;
    else if( dim == 4 )
        glVertexFunc = glVertex4fv;

    // These are number of _floats_ per item, not _bytes_ per item as opengl uses.
    int vsize, tsize, csize;
    vsize = tsize = csize = stride / sizeof(float);
    if( !stride )
        {
        // 0 means that arrays are tightly packed. This gives us different
        //  strides for different arrays
        vsize = dim;
        tsize = 2;
        csize = 4;
        }

    glBegin( GL_QUADS );
    // This sucks. But makes it faster.
    if( texture && color )
        {
        for( int i = 0; i < count; i++ )
            {
            glTexCoord2fv( texture + i*tsize );
            glColor4fv( color + i*csize );
            glVertexFunc( vertices + i*vsize );
            }
        }
    else if( texture )
        {
        for( int i = 0; i < count; i++ )
            {
            glTexCoord2fv( texture + i*tsize );
            glVertexFunc( vertices + i*vsize );
            }
        }
    else if( color )
        {
        for( int i = 0; i < count; i++ )
            {
            glColor4fv( color + i*csize );
            glVertexFunc( vertices + i*vsize );
            }
        }
    else
        {
        for( int i = 0; i < count; i++ )
            glVertexFunc( vertices + i*vsize );
        }
    glEnd();
}

void addQuadVertices(QVector<float>& verts, float x1, float y1, float x2, float y2)
{
    verts << x1 << y1;
    verts << x1 << y2;
    verts << x2 << y2;
    verts << x2 << y1;
}

void renderRoundBox( const QRect& area, float roundness, GLTexture* texture )
{
    static GLTexture* circleTexture = 0;
    if( !texture && !circleTexture )
        {
        QString texturefile =  KGlobal::dirs()->findResource("data", "kwin/circle.png");
        circleTexture = new GLTexture(texturefile);
        }
    if( !texture )
        {
        texture = circleTexture;
        }

    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glPushMatrix();

    QVector<float> verts, texcoords;
    verts.reserve(80);
    texcoords.reserve(80);
    // center
    addQuadVertices(verts, area.left() + roundness, area.top() + roundness, area.right() - roundness, area.bottom() - roundness);
    addQuadVertices(texcoords, 0.5, 0.5, 0.5, 0.5);
    // sides
    // left
    addQuadVertices(verts, area.left(), area.top() + roundness, area.left() + roundness, area.bottom() - roundness);
    addQuadVertices(texcoords, 0.0, 0.5, 0.5, 0.5);
    // top
    addQuadVertices(verts, area.left() + roundness, area.top(), area.right() - roundness, area.top() + roundness);
    addQuadVertices(texcoords, 0.5, 0.0, 0.5, 0.5);
    // right
    addQuadVertices(verts, area.right() - roundness, area.top() + roundness, area.right(), area.bottom() - roundness);
    addQuadVertices(texcoords, 0.5, 0.5, 1.0, 0.5);
    // bottom
    addQuadVertices(verts, area.left() + roundness, area.bottom() - roundness, area.right() - roundness, area.bottom());
    addQuadVertices(texcoords, 0.5, 0.5, 0.5, 1.0);
    // corners
    // top-left
    addQuadVertices(verts, area.left(), area.top(), area.left() + roundness, area.top() + roundness);
    addQuadVertices(texcoords, 0.0, 0.0, 0.5, 0.5);
    // top-right
    addQuadVertices(verts, area.right() - roundness, area.top(), area.right(), area.top() + roundness);
    addQuadVertices(texcoords, 0.5, 0.0, 1.0, 0.5);
    // bottom-left
    addQuadVertices(verts, area.left(), area.bottom() - roundness, area.left() + roundness, area.bottom());
    addQuadVertices(texcoords, 0.0, 0.5, 0.5, 1.0);
    // bottom-right
    addQuadVertices(verts, area.right() - roundness, area.bottom() - roundness, area.right(), area.bottom());
    addQuadVertices(texcoords, 0.5, 0.5, 1.0, 1.0);

    texture->bind();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    // We have two elements per vertex in the verts array
    int verticesCount = verts.count() / 2;
    renderGLGeometry( verticesCount, verts.data(), texcoords.data() );
    texture->unbind();

    glPopMatrix();
    glPopAttrib();
}

void renderRoundBoxWithEdge( const QRect& area, float roundness )
{
    static GLTexture* texture = 0;
    if( !texture )
    {
        QString texturefile =  KGlobal::dirs()->findResource("data", "kwin/circle-edgy.png");
        texture = new GLTexture(texturefile);
    }
    renderRoundBox( area, roundness, texture );
}

//****************************************
// GLTexture
//****************************************

bool GLTexture::mNPOTTextureSupported = false;
bool GLTexture::mFramebufferObjectSupported = false;
bool GLTexture::mSaturationSupported = false;

GLTexture::GLTexture()
    {
    init();
    }

GLTexture::GLTexture( const QImage& image, GLenum target )
    {
    init();
    load( image, target );
    }

GLTexture::GLTexture( const QPixmap& pixmap, GLenum target )
    {
    init();
    load( pixmap, target );
    }

GLTexture::GLTexture( const QString& fileName )
    {
    init();
    load( fileName );
    }

GLTexture::GLTexture( int width, int height )
    {
    init();

    if( NPOTTextureSupported() || ( isPowerOfTwo( width ) && isPowerOfTwo( height )))
        {
        mTarget = GL_TEXTURE_2D;
        mScale.setWidth( 1.0 / width);
        mScale.setHeight( 1.0 / height);
        can_use_mipmaps = true;

        glGenTextures( 1, &mTexture );
        bind();
        glTexImage2D( mTarget, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        unbind();
        }
    }

GLTexture::~GLTexture()
    {
    discard();
    }

void GLTexture::init()
    {
    mTexture = None;
    mTarget = 0;
    mFilter = 0;
    y_inverted = false;
    can_use_mipmaps = false;
    has_valid_mipmaps = false;
    }

void GLTexture::initStatic()
    {
    mNPOTTextureSupported = hasGLExtension( "GL_ARB_texture_non_power_of_two" );
    mFramebufferObjectSupported = hasGLExtension( "GL_EXT_framebuffer_object" );
    mSaturationSupported = ((hasGLExtension("GL_ARB_texture_env_crossbar")
        && hasGLExtension("GL_ARB_texture_env_dot3")) || hasGLVersion(1, 4))
        && (glTextureUnitsCount >= 4) && glActiveTexture != NULL;
    }

bool GLTexture::isNull() const
    {
    return mTexture == None;
    }

bool GLTexture::load( const QImage& image, GLenum target )
    {
    if( image.isNull())
        return false;
    QImage img = image;
    mTarget = target;
    if( mTarget != GL_TEXTURE_RECTANGLE_ARB )
        {
        if( !NPOTTextureSupported()
            && ( !isPowerOfTwo( image.width()) || !isPowerOfTwo( image.height())))
            { // non-rectangular target requires POT texture
            img = img.scaled( nearestPowerOfTwo( image.width()),
                    nearestPowerOfTwo( image.height()));
            }
        mScale.setWidth( 1.0 / img.width());
        mScale.setHeight( 1.0 / img.height());
        can_use_mipmaps = true;
        }
    else
        {
        mScale.setWidth( 1.0 );
        mScale.setHeight( 1.0 );
        can_use_mipmaps = false;
        }
    setFilter( GL_LINEAR );
    mSize = img.size();
    y_inverted = false;

    img = convertToGLFormat( img );

    setDirty();
    if( isNull())
        glGenTextures( 1, &mTexture );
    bind();
    glTexImage2D( mTarget, 0, GL_RGBA, img.width(), img.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    unbind();
    return true;
    }

bool GLTexture::load( const QPixmap& pixmap, GLenum target )
    {
    if( pixmap.isNull())
        return false;
    return load( pixmap.toImage(), target );
    }

bool GLTexture::load( const QString& fileName )
    {
    if( fileName.isEmpty())
        return false;
    return load( QImage( fileName ));
    }

void GLTexture::discard()
    {
    setDirty();
    if( mTexture != None )
        glDeleteTextures( 1, &mTexture );
    mTexture = None;
    }

void GLTexture::bind()
    {
    glEnable( mTarget );
    glBindTexture( mTarget, mTexture );
    enableFilter();
    }

void GLTexture::unbind()
    {
    glBindTexture( mTarget, 0 );
    glDisable( mTarget );
    }

void GLTexture::render( int mask, QRegion region, const QRect& rect )
    {
    return render( !( mask & ( Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_SCREEN_TRANSFORMED )),
        region, rect );
    }

void GLTexture::render( bool clip, QRegion region, const QRect& rect )
    {
    const float verts[ 4 * 2 ] =
        {
        rect.x(), rect.y(),
        rect.x(), rect.y() + rect.height(),
        rect.x() + rect.width(), rect.y() + rect.height(),
        rect.x() + rect.width(), rect.y()
        };
    const float texcoords[ 4 * 2 ] =
        {
        0, 1,
        0, 0,
        1, 0,
        1, 1
        };
    renderGLGeometry( clip, region, 4, verts, texcoords );
    }

void GLTexture::enableUnnormalizedTexCoords()
    {
    // update texture matrix to handle GL_TEXTURE_2D and GL_TEXTURE_RECTANGLE
    glMatrixMode( GL_TEXTURE );
    glPushMatrix();
    glLoadIdentity();
    glScalef( mScale.width(), mScale.height(), 1 );
    if( !y_inverted )
        {
        // Modify texture matrix so that we could always use non-opengl
        //  coordinates for textures
        glScalef( 1, -1, 1 );
        glTranslatef( 0, -mSize.height(), 0 );
        }
    glMatrixMode( GL_MODELVIEW );
    }

void GLTexture::disableUnnormalizedTexCoords()
    {
    // Restore texture matrix
    glMatrixMode( GL_TEXTURE );
    glPopMatrix();
    glMatrixMode( GL_MODELVIEW );
    }

GLuint GLTexture::texture() const
    {
    return mTexture;
    }

GLenum GLTexture::target() const
    {
    return mTarget;
    }

GLenum GLTexture::filter() const
    {
    return mFilter;
    }

bool GLTexture::isDirty() const
    {
    return has_valid_mipmaps;
    }

void GLTexture::setTexture( GLuint texture )
    {
    discard();
    mTexture = texture;
    }

void GLTexture::setTarget( GLenum target )
    {
    mTarget = target;
    }

void GLTexture::setFilter( GLenum filter )
    {
    mFilter = filter;
    }

void GLTexture::setWrapMode( GLenum mode )
    {
    bind();
    glTexParameteri( mTarget, GL_TEXTURE_WRAP_S, mode );
    glTexParameteri( mTarget, GL_TEXTURE_WRAP_T, mode );
    unbind();
    }

void GLTexture::setDirty()
    {
    has_valid_mipmaps = false;
    }


void GLTexture::enableFilter()
    {
    if( mFilter == GL_LINEAR_MIPMAP_LINEAR )
        { // trilinear filtering requested, but is it possible?
        if( NPOTTextureSupported()
            && framebufferObjectSupported()
            && can_use_mipmaps )
            {
            glTexParameteri( mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
            glTexParameteri( mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
            if( !has_valid_mipmaps )
                {
                glGenerateMipmap( mTarget );
                has_valid_mipmaps = true;
                }
            }
        else
            { // can't use trilinear, so use bilinear
            setFilter( GL_LINEAR );
            glTexParameteri( mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
            glTexParameteri( mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
            }
        }
    else if( mFilter == GL_LINEAR )
        {
        glTexParameteri( mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        }
    else
        { // if neither trilinear nor bilinear, default to fast filtering
        setFilter( GL_NEAREST );
        glTexParameteri( mTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        glTexParameteri( mTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        }
    }

QImage GLTexture::convertToGLFormat( const QImage& img ) const
    {
    // This method has been copied from Qt's QGLWidget::convertToGLFormat()
    QImage res = img.convertToFormat(QImage::Format_ARGB32);
    res = res.mirrored();

    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // Qt has ARGB; OpenGL wants RGBA
        for (int i=0; i < res.height(); i++) {
            uint *p = (uint*)res.scanLine(i);
            uint *end = p + res.width();
            while (p < end) {
                *p = (*p << 8) | ((*p >> 24) & 0xFF);
                p++;
            }
        }
    }
    else {
        // Qt has ARGB; OpenGL wants ABGR (i.e. RGBA backwards)
        res = res.rgbSwapped();
    }
    return res;
    }

//****************************************
// GLShader
//****************************************

bool GLShader::mFragmentShaderSupported = false;
bool GLShader::mVertexShaderSupported = false;

void GLShader::initStatic()
{
    mFragmentShaderSupported = mVertexShaderSupported =
            hasGLExtension("GL_ARB_shader_objects") && hasGLExtension("GL_ARB_shading_language_100");
    mVertexShaderSupported &= hasGLExtension("GL_ARB_vertex_shader");
    mFragmentShaderSupported &= hasGLExtension("GL_ARB_fragment_shader");
}


GLShader::GLShader(const QString& vertexfile, const QString& fragmentfile)
    {
    mValid = false;
    mVariableLocations = 0;
    mProgram = 0;

    loadFromFiles(vertexfile, fragmentfile);
    }

GLShader::~GLShader()
{
    if(mVariableLocations)
    {
        mVariableLocations->clear();
        delete mVariableLocations;
    }

    if(mProgram)
    {
        glDeleteProgram(mProgram);
    }
}

bool GLShader::loadFromFiles(const QString& vertexfile, const QString& fragmentfile)
    {
    QFile vf(vertexfile);
    if(!vf.open(QIODevice::ReadOnly))
        {
        kError(1212) << "Couldn't open '" << vertexfile << "' for reading!" << endl;
        return false;
        }
    QString vertexsource(vf.readAll());

    QFile ff(fragmentfile);
    if(!ff.open(QIODevice::ReadOnly))
        {
        kError(1212) << "Couldn't open '" << fragmentfile << "' for reading!" << endl;
        return false;
        }
    QString fragsource(ff.readAll());

    return load(vertexsource, fragsource);
    }

bool GLShader::load(const QString& vertexsource, const QString& fragmentsource)
    {
    // Make sure shaders are actually supported
    if(( !vertexsource.isEmpty() && !vertexShaderSupported() ) ||
          ( !fragmentsource.isEmpty() && !fragmentShaderSupported() ))
        {
        kDebug(1212) << "Shaders not supported";
        return false;
        }

    GLuint vertexshader;
    GLuint fragmentshader;

    GLsizei logsize, logarraysize;
    char* log = 0;

    // Create program object
    mProgram = glCreateProgram();
    if(!vertexsource.isEmpty())
        {
        // Create shader object
        vertexshader = glCreateShader(GL_VERTEX_SHADER);
        // Load it
        const QByteArray& srcba = vertexsource.toLatin1();
        const char* src = srcba.data();
        glShaderSource(vertexshader, 1, &src, NULL);
        // Compile the shader
        glCompileShader(vertexshader);
        // Make sure it compiled correctly
        int compiled;
        glGetShaderiv(vertexshader, GL_COMPILE_STATUS, &compiled);
        // Get info log
        glGetShaderiv(vertexshader, GL_INFO_LOG_LENGTH, &logarraysize);
        log = new char[logarraysize];
        glGetShaderInfoLog(vertexshader, logarraysize, &logsize, log);
        if(!compiled)
            {
            kError(1212) << "Couldn't compile vertex shader! Log:" << endl << log << endl;
            delete[] log;
            return false;
            }
        else if(logsize > 0)
            kDebug(1212) << "Vertex shader compilation log:"<< log;
        // Attach the shader to the program
        glAttachShader(mProgram, vertexshader);
        // Delete shader
        glDeleteShader(vertexshader);
        delete[] log;
        }


    if(!fragmentsource.isEmpty())
        {
        fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
        // Load it
        const QByteArray& srcba = fragmentsource.toLatin1();
        const char* src = srcba.data();
        glShaderSource(fragmentshader, 1, &src, NULL);
        //glShaderSource(fragmentshader, 1, &fragmentsrc.latin1(), NULL);
        // Compile the shader
        glCompileShader(fragmentshader);
        // Make sure it compiled correctly
        int compiled;
        glGetShaderiv(fragmentshader, GL_COMPILE_STATUS, &compiled);
        // Get info log
        glGetShaderiv(fragmentshader, GL_INFO_LOG_LENGTH, &logarraysize);
        log = new char[logarraysize];
        glGetShaderInfoLog(fragmentshader, logarraysize, &logsize, log);
        if(!compiled)
            {
            kError(1212) << "Couldn't compile fragment shader! Log:" << endl << log << endl;
            delete[] log;
            return false;
            }
        else if(logsize > 0)
            kDebug(1212) << "Fragment shader compilation log:"<< log;
        // Attach the shader to the program
        glAttachShader(mProgram, fragmentshader);
        // Delete shader
        glDeleteShader(fragmentshader);
        delete[] log;
        }


    // Link the program
    glLinkProgram(mProgram);
    // Make sure it linked correctly
    int linked;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
    // Get info log
    glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &logarraysize);
    log = new char[logarraysize];
    glGetProgramInfoLog(mProgram, logarraysize, &logsize, log);
    if(!linked)
        {
        kError(1212) << "Couldn't link the program! Log" << endl << log << endl;
        delete[] log;
        return false;
        }
    else if(logsize > 0)
        kDebug(1212) << "Shader linking log:"<< log;
    delete[] log;

    mVariableLocations = new QHash<QString, int>;

    mValid = true;
    return true;
    }

void GLShader::bind()
    {
    glUseProgram(mProgram);
    }

void GLShader::unbind()
    {
    glUseProgram(0);
    }

int GLShader::uniformLocation(const QString& name)
    {
    if(!mVariableLocations)
        {
        return -1;
        }
    if(!mVariableLocations->contains(name))
        {
        int location = glGetUniformLocation(mProgram, name.toLatin1().data());
        mVariableLocations->insert(name, location);
        }
    return mVariableLocations->value(name);
    }

bool GLShader::setUniform(const QString& name, float value)
    {
    int location = uniformLocation(name);
    if(location >= 0)
        {
        glUniform1f(location, value);
        }
    return (location >= 0);
    }

bool GLShader::setUniform(const QString& name, int value)
    {
    int location = uniformLocation(name);
    if(location >= 0)
        {
        glUniform1i(location, value);
        }
    return (location >= 0);
    }

int GLShader::attributeLocation(const QString& name)
    {
    if(!mVariableLocations)
        {
        return -1;
        }
    if(!mVariableLocations->contains(name))
        {
        int location = glGetAttribLocation(mProgram, name.toLatin1().data());
        mVariableLocations->insert(name, location);
        }
    return mVariableLocations->value(name);
    }

bool GLShader::setAttribute(const QString& name, float value)
    {
    int location = attributeLocation(name);
    if(location >= 0)
        {
        glVertexAttrib1f(location, value);
        }
    return (location >= 0);
    }



/***  GLRenderTarget  ***/
bool GLRenderTarget::mSupported = false;

void GLRenderTarget::initStatic()
    {
    mSupported = hasGLExtension("GL_EXT_framebuffer_object") && glFramebufferTexture2D;
    }

GLRenderTarget::GLRenderTarget(GLTexture* color)
    {
    // Reset variables
    mValid = false;

    mTexture = color;

    // Make sure FBO is supported
    if(mSupported && mTexture && !mTexture->isNull())
        {
        initFBO();
        }
    else
        kError(1212) << "Render targets aren't supported!" << endl;
    }

GLRenderTarget::~GLRenderTarget()
    {
    if(mValid)
        {
        glDeleteFramebuffers(1, &mFramebuffer);
        }
    }

bool GLRenderTarget::enable()
    {
    if(!valid())
        {
        kError(1212) << "Can't enable invalid render target!" << endl;
        return false;
        }

    glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFramebuffer);
    mTexture->setDirty();

    return true;
    }

bool GLRenderTarget::disable()
    {
    if(!valid())
        {
        kError(1212) << "Can't disable invalid render target!" << endl;
        return false;
        }

    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    mTexture->setDirty();

    return true;
    }

void GLRenderTarget::initFBO()
    {
    glGenFramebuffers(1, &mFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFramebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                           mTexture->target(), mTexture->texture(), 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
    if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
        {
        kError(1212) << "Invalid fb status: " << status << endl;
        }

    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

    mValid = true;
    }

} // namespace

#endif
