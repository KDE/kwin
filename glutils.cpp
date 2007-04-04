/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "glutils.h"

#include <kdebug.h>
#include <QPixmap>
#include <QImage>
#include <QHash>
#include <QFile>


#define MAKE_GL_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )


namespace KWinInternal
{
// Variables
// GL version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glVersion;
// GLX version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glXVersion;
// List of all supported GL and GLX extensions
static QStringList glExtensions;

int glTextureUnitsCount;


// Functions
void initGLX()
    {
#ifdef HAVE_OPENGL
    // Get GLX version
    int major, minor;
    glXQueryVersion( display(), &major, &minor );
    glXVersion = MAKE_GL_VERSION( major, minor, 0 );
    // Get list of supported GLX extensions. Simply add it to the list of OpenGL extensions.
    glExtensions += QString((const char*)glXQueryExtensionsString(
        display(), DefaultScreen( display()))).split(" ");

    glxResolveFunctions();
#else
    glXVersion = MAKE_GL_VERSION( 0, 0, 0 );
#endif
    }

void initGL()
    {
#ifdef HAVE_OPENGL
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
#else
    glVersion = MAKE_GL_VERSION( 0, 0, 0 );
#endif
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
    return glExtensions.contains(extension);
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

#ifdef HAVE_OPENGL

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

    loadFromFiles(vertexfile, fragmentfile);
    }

bool GLShader::loadFromFiles(const QString& vertexfile, const QString& fragmentfile)
    {
    QFile vf(vertexfile);
    if(!vf.open(IO_ReadOnly))
        {
        kError(1212) << k_funcinfo << "Couldn't open '" << vertexfile << "' for reading!" << endl;
        return false;
        }
    QString vertexsource(vf.readAll());

    QFile ff(fragmentfile);
    if(!ff.open(IO_ReadOnly))
        {
        kError(1212) << k_funcinfo << "Couldn't open '" << fragmentfile << "' for reading!" << endl;
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
        kDebug(1212) << k_funcinfo << "Shaders not supported" << endl;
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
        glGetShaderiv(fragmentshader, GL_COMPILE_STATUS, &compiled);
        // Get info log
        glGetShaderiv(vertexshader, GL_INFO_LOG_LENGTH, &logarraysize);
        log = new char[logarraysize];
        glGetShaderInfoLog(vertexshader, logarraysize, &logsize, log);
        if(!compiled)
            {
            kError(1212) << k_funcinfo << "Couldn't compile vertex shader! Log:" << endl << log << endl;
            delete[] log;
            return false;
            }
        else if(logsize > 0)
            kDebug(1212) << "Vertex shader compilation log:" << endl << log << endl;
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
            kError(1212) << k_funcinfo << "Couldn't compile fragment shader! Log:" << endl << log << endl;
            delete[] log;
            return false;
            }
        else if(logsize > 0)
            kDebug(1212) << "Fragment shader compilation log:" << endl << log << endl;
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
        kError(1212) << k_funcinfo << "Couldn't link the program! Log" << endl << log << endl;
        delete[] log;
        return false;
        }
    else if(logsize > 0)
        kDebug(1212) << "Shader linking log:" << endl << log << endl;
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

#endif

} // namespace
