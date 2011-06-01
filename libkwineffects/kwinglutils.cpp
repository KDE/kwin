/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#include "kwinglplatform.h"

#include "kdebug.h"
#include <kstandarddirs.h>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>

#include <QPixmap>
#include <QImage>
#include <QHash>
#include <QFile>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include <math.h>

#define DEBUG_GLRENDERTARGET 0

#define MAKE_GL_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )

namespace KWin
{
// Variables
// GL version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glVersion;
// GLX version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glXVersion;
// EGL version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int eglVersion;
// List of all supported GL, EGL and GLX extensions
static QStringList glExtensions;
static QStringList glxExtensions;
static QStringList eglExtension;
static bool legacyGl;

int glTextureUnitsCount;


// Functions
void initGLX()
{
#ifndef KWIN_HAVE_OPENGLES
    // Get GLX version
    int major, minor;
    glXQueryVersion(display(), &major, &minor);
    glXVersion = MAKE_GL_VERSION(major, minor, 0);
    // Get list of supported GLX extensions
    glxExtensions = QString((const char*)glXQueryExtensionsString(
                                display(), DefaultScreen(display()))).split(' ');

    glxResolveFunctions();
#endif
}

void initEGL()
{
#ifdef KWIN_HAVE_OPENGLES
    EGLDisplay dpy = eglGetCurrentDisplay();
    int major, minor;
    eglInitialize(dpy, &major, &minor);
    eglVersion = MAKE_GL_VERSION(major, minor, 0);
    eglExtension = QString((const char*)eglQueryString(dpy, EGL_EXTENSIONS)).split(' ');

    eglResolveFunctions();
#endif
}

void initGL()
{
    // Get OpenGL version
    QString glversionstring = QString((const char*)glGetString(GL_VERSION));
    QStringList glversioninfo = glversionstring.left(glversionstring.indexOf(' ')).split('.');
    while (glversioninfo.count() < 3)
        glversioninfo << "0";
#ifdef KWIN_HAVE_OPENGLES
    legacyGl = false;
#else
    KSharedConfig::Ptr kwinconfig = KSharedConfig::openConfig("kwinrc", KConfig::NoGlobals);
    KConfigGroup config(kwinconfig, "Compositing");
    legacyGl = config.readEntry<bool>("GLLegacy", false);
    glVersion = MAKE_GL_VERSION(glversioninfo[0].toInt(), glversioninfo[1].toInt(), glversioninfo[2].toInt());
#endif
    // Get list of supported OpenGL extensions
    glExtensions = QString((const char*)glGetString(GL_EXTENSIONS)).split(' ');

    // handle OpenGL extensions functions
    glResolveFunctions();

    GLTexture::initStatic();
    GLRenderTarget::initStatic();
    GLVertexBuffer::initStatic();
}

void cleanupGL()
{
    ShaderManager::cleanup();
}

bool hasGLVersion(int major, int minor, int release)
{
    return glVersion >= MAKE_GL_VERSION(major, minor, release);
}

bool hasGLXVersion(int major, int minor, int release)
{
    return glXVersion >= MAKE_GL_VERSION(major, minor, release);
}

bool hasEGLVersion(int major, int minor, int release)
{
    return eglVersion >= MAKE_GL_VERSION(major, minor, release);
}

bool hasGLExtension(const QString& extension)
{
    return glExtensions.contains(extension) || glxExtensions.contains(extension) || eglExtension.contains(extension);
}

static QString formatGLError(GLenum err)
{
    switch(err) {
    case GL_NO_ERROR:          return "GL_NO_ERROR";
    case GL_INVALID_ENUM:      return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:     return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
#ifndef KWIN_HAVE_OPENGLES
    case GL_STACK_OVERFLOW:    return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:   return "GL_STACK_UNDERFLOW";
#endif
    case GL_OUT_OF_MEMORY:     return "GL_OUT_OF_MEMORY";
    default: return QString("0x") + QString::number(err, 16);
    }
}

bool checkGLError(const char* txt)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        kWarning(1212) << "GL error (" << txt << "): " << formatGLError(err);
        return true;
    }
    return false;
}

int nearestPowerOfTwo(int x)
{
    // This method had been copied from Qt's nearest_gl_texture_size()
    int n = 0, last = 0;
    for (int s = 0; s < 32; ++s) {
        if (((x >> s) & 1) == 1) {
            ++n;
            last = s;
        }
    }
    if (n > 1)
        return 1 << (last + 1);
    return 1 << last;
}

void pushMatrix()
{
#ifndef KWIN_HAVE_OPENGLES
    glPushMatrix();
#endif
}

void pushMatrix(const QMatrix4x4 &matrix)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(matrix)
#else
    glPushMatrix();
    multiplyMatrix(matrix);
#endif
}

void multiplyMatrix(const QMatrix4x4 &matrix)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(matrix)
#else
    GLfloat m[16];
    const qreal *data = matrix.constData();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i*4+j] = data[i*4+j];
        }
    }
    glMultMatrixf(m);
#endif
}

void loadMatrix(const QMatrix4x4 &matrix)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(matrix)
#else
    GLfloat m[16];
    const qreal *data = matrix.constData();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i*4+j] = data[i*4+j];
        }
    }
    glLoadMatrixf(m);
#endif
}

void popMatrix()
{
#ifndef KWIN_HAVE_OPENGLES
    glPopMatrix();
#endif
}

//****************************************
// GLTexture
//****************************************

bool GLTexture::sNPOTTextureSupported = false;
bool GLTexture::sFramebufferObjectSupported = false;
bool GLTexture::sSaturationSupported = false;

GLTexture::GLTexture()
{
    init();
}

GLTexture::GLTexture(const QImage& image, GLenum target)
{
    init();
    load(image, target);
}

GLTexture::GLTexture(const QPixmap& pixmap, GLenum target)
{
    init();
    load(pixmap, target);
}

GLTexture::GLTexture(const QString& fileName)
{
    init();
    load(fileName);
}

GLTexture::GLTexture(int width, int height)
{
    init();

    if (NPOTTextureSupported() || (isPowerOfTwo(width) && isPowerOfTwo(height))) {
        mTarget = GL_TEXTURE_2D;
        mScale.setWidth(1.0 / width);
        mScale.setHeight(1.0 / height);
        mSize = QSize(width, height);
        can_use_mipmaps = true;

        glGenTextures(1, &mTexture);
        bind();
#ifdef KWIN_HAVE_OPENGLES
        // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
        // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
        glTexImage2D(mTarget, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#else
        glTexImage2D(mTarget, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
#endif
        unbind();
    }
}

GLTexture::~GLTexture()
{
    delete m_vbo;
    discard();
    assert(mUnnormalizeActive == 0);
    assert(mNormalizeActive == 0);
}

void GLTexture::init()
{
    mTexture = None;
    mTarget = 0;
    mFilter = 0;
    y_inverted = false;
    can_use_mipmaps = false;
    has_valid_mipmaps = false;
    mUnnormalizeActive = 0;
    mNormalizeActive = 0;
    m_vbo = 0;
}

void GLTexture::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    sNPOTTextureSupported = true;
    sFramebufferObjectSupported = true;
    sSaturationSupported = true;
#else
    sNPOTTextureSupported = hasGLExtension("GL_ARB_texture_non_power_of_two");
    sFramebufferObjectSupported = hasGLExtension("GL_EXT_framebuffer_object");
    sSaturationSupported = ((hasGLExtension("GL_ARB_texture_env_crossbar")
                             && hasGLExtension("GL_ARB_texture_env_dot3")) || hasGLVersion(1, 4))
                           && (glTextureUnitsCount >= 4) && glActiveTexture != NULL;
#endif
}

bool GLTexture::isNull() const
{
    return mTexture == None;
}

QSize GLTexture::size() const
{
    return mSize;
}

bool GLTexture::load(const QImage& image, GLenum target)
{
    if (image.isNull())
        return false;
    QImage img = image;
    mTarget = target;
#ifndef KWIN_HAVE_OPENGLES
    if (mTarget != GL_TEXTURE_RECTANGLE_ARB) {
#endif
        if (!NPOTTextureSupported()
                && (!isPowerOfTwo(image.width()) || !isPowerOfTwo(image.height()))) {
            // non-rectangular target requires POT texture
            img = img.scaled(nearestPowerOfTwo(image.width()),
                             nearestPowerOfTwo(image.height()));
        }
        mScale.setWidth(1.0 / img.width());
        mScale.setHeight(1.0 / img.height());
        can_use_mipmaps = true;
#ifndef KWIN_HAVE_OPENGLES
    } else {
        mScale.setWidth(1.0);
        mScale.setHeight(1.0);
        can_use_mipmaps = false;
    }
#endif
    setFilter(GL_LINEAR);
    mSize = img.size();
    y_inverted = false;

    img = convertToGLFormat(img);

    setDirty();
    if (isNull())
        glGenTextures(1, &mTexture);
    bind();
#ifdef KWIN_HAVE_OPENGLES
    // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
    // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
    glTexImage2D(mTarget, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
#else
    glTexImage2D(mTarget, 0, GL_RGBA8, img.width(), img.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, img.bits());
#endif
    unbind();
    return true;
}

bool GLTexture::load(const QPixmap& pixmap, GLenum target)
{
    if (pixmap.isNull())
        return false;
    return load(pixmap.toImage(), target);
}

bool GLTexture::load(const QString& fileName)
{
    if (fileName.isEmpty())
        return false;
    return load(QImage(fileName));
}

void GLTexture::discard()
{
    setDirty();
    if (mTexture != None)
        glDeleteTextures(1, &mTexture);
    mTexture = None;
}

void GLTexture::bind()
{
#ifndef KWIN_HAVE_OPENGLES
    glEnable(mTarget);
#endif
    glBindTexture(mTarget, mTexture);
    enableFilter();
}

void GLTexture::unbind()
{
    glBindTexture(mTarget, 0);
#ifndef KWIN_HAVE_OPENGLES
    glDisable(mTarget);
#endif
}

void GLTexture::render(QRegion region, const QRect& rect)
{
    if (rect.size() != m_cachedSize) {
        m_cachedSize = rect.size();
        QRect r(rect);
        r.moveTo(0, 0);
        if (!m_vbo) {
            m_vbo = new GLVertexBuffer(KWin::GLVertexBuffer::Static);
        }
        const float verts[ 4 * 2 ] = {
            // NOTICE: r.x/y could be replaced by "0", but that would make it unreadable...
            r.x(), r.y(),
            r.x(), r.y() + rect.height(),
            r.x() + rect.width(), r.y(),
            r.x() + rect.width(), r.y() + rect.height()
        };
        const float texcoords[ 4 * 2 ] = {
            0.0f, y_inverted ? 0.0f : 1.0f, // y needs to be swapped (normalized coords)
            0.0f, y_inverted ? 1.0f : 0.0f,
            1.0f, y_inverted ? 0.0f : 1.0f,
            1.0f, y_inverted ? 1.0f : 0.0f
        };
        m_vbo->setData(4, 2, verts, texcoords);
    }
    QMatrix4x4 translation;
    translation.translate(rect.x(), rect.y());
    if (ShaderManager::instance()->isShaderBound()) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::Offset, QVector2D(rect.x(), rect.y()));
        shader->setUniform(GLShader::WindowTransformation, translation);
        shader->setUniform(GLShader::TextureWidth, 1.0f);
        shader->setUniform(GLShader::TextureHeight, 1.0f);
    } else {
        pushMatrix(translation);
    }
    m_vbo->render(region, GL_TRIANGLE_STRIP);
    if (ShaderManager::instance()->isShaderBound()) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
    } else {
        popMatrix();
    }
}

void GLTexture::enableUnnormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    assert(mNormalizeActive == 0);
    if (mUnnormalizeActive++ != 0)
        return;
    // update texture matrix to handle GL_TEXTURE_2D and GL_TEXTURE_RECTANGLE
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(mScale.width(), mScale.height(), 1);
    if (!y_inverted) {
        // Modify texture matrix so that we could always use non-opengl
        //  coordinates for textures
        glScalef(1, -1, 1);
        glTranslatef(0, -mSize.height(), 0);
    }
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::disableUnnormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    if (--mUnnormalizeActive != 0)
        return;
    // Restore texture matrix
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::enableNormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    assert(mUnnormalizeActive == 0);
    if (mNormalizeActive++ != 0)
        return;
    // update texture matrix to handle GL_TEXTURE_2D and GL_TEXTURE_RECTANGLE
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(mSize.width() * mScale.width(), mSize.height() * mScale.height(), 1);
    if (y_inverted) {
        // Modify texture matrix so that we could always use non-opengl
        //  coordinates for textures
        glScalef(1, -1, 1);
        glTranslatef(0, -1, 0);
    }
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::disableNormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    if (--mNormalizeActive != 0)
        return;
    // Restore texture matrix
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
#endif
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

void GLTexture::setTexture(GLuint texture)
{
    discard();
    mTexture = texture;
}

void GLTexture::setTarget(GLenum target)
{
    mTarget = target;
}

void GLTexture::setFilter(GLenum filter)
{
    mFilter = filter;
}

void GLTexture::setWrapMode(GLenum mode)
{
    bind();
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, mode);
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, mode);
    unbind();
}

void GLTexture::setDirty()
{
    has_valid_mipmaps = false;
}


void GLTexture::enableFilter()
{
    if (mFilter == GL_LINEAR_MIPMAP_LINEAR) {
        // trilinear filtering requested, but is it possible?
        if (NPOTTextureSupported()
                && framebufferObjectSupported()
                && can_use_mipmaps) {
            glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            if (!has_valid_mipmaps) {
                glGenerateMipmap(mTarget);
                has_valid_mipmaps = true;
            }
        } else {
            // can't use trilinear, so use bilinear
            setFilter(GL_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    } else if (mFilter == GL_LINEAR) {
        glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        // if neither trilinear nor bilinear, default to fast filtering
        setFilter(GL_NEAREST);
        glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

static void convertToGLFormatHelper(QImage &dst, const QImage &img, GLenum texture_format)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(texture_format)
#endif
    // Copied from Qt
    Q_ASSERT(dst.size() == img.size());
    Q_ASSERT(dst.depth() == 32);
    Q_ASSERT(img.depth() == 32);

    const int width = img.width();
    const int height = img.height();
    const uint *p = (const uint*) img.scanLine(img.height() - 1);
    uint *q = (uint*) dst.scanLine(0);

#ifndef KWIN_HAVE_OPENGLES
    if (texture_format == GL_BGRA) {
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            // mirror + swizzle
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = ((*p << 24) & 0xff000000)
                         | ((*p >> 24) & 0x000000ff)
                         | ((*p << 8) & 0x00ff0000)
                         | ((*p >> 8) & 0x0000ff00);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        } else {
            const uint bytesPerLine = img.bytesPerLine();
            for (int i = 0; i < height; ++i) {
                memcpy(q, p, bytesPerLine);
                q += width;
                p -= width;
            }
        }
    } else {
#endif
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = (*p << 8) | ((*p >> 24) & 0xFF);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        } else {
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = ((*p << 16) & 0xff0000) | ((*p >> 16) & 0xff) | (*p & 0xff00ff00);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        }
#ifndef KWIN_HAVE_OPENGLES
    }
#endif
}

QImage GLTexture::convertToGLFormat(const QImage& img) const
{
    // Copied from Qt's QGLWidget::convertToGLFormat()
    QImage res(img.size(), QImage::Format_ARGB32);
#ifdef KWIN_HAVE_OPENGLES
    convertToGLFormatHelper(res, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), GL_RGBA);
#else
    convertToGLFormatHelper(res, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), GL_BGRA);
#endif
    return res;
}

//****************************************
// GLShader
//****************************************

GLShader::GLShader()
    : mProgram(0)
    , mValid(false)
    , mLocationsResolved(false)
    , mTextureWidth(-1.0f)
    , mTextureHeight(-1.0f)
{
}

GLShader::GLShader(const QString& vertexfile, const QString& fragmentfile)
    : mProgram(0)
    , mValid(false)
    , mLocationsResolved(false)
    , mTextureWidth(-1.0f)
    , mTextureHeight(-1.0f)
{
    loadFromFiles(vertexfile, fragmentfile);
}

GLShader::~GLShader()
{
    if (mProgram) {
        glDeleteProgram(mProgram);
    }
}

bool GLShader::loadFromFiles(const QString &vertexFile, const QString &fragmentFile)
{
    QFile vf(vertexFile);
    if (!vf.open(QIODevice::ReadOnly)) {
        kError(1212) << "Couldn't open" << vertexFile << "for reading!" << endl;
        return false;
    }
    const QByteArray vertexSource = vf.readAll();

    QFile ff(fragmentFile);
    if (!ff.open(QIODevice::ReadOnly)) {
        kError(1212) << "Couldn't open" << fragmentFile << "for reading!" << endl;
        return false;
    }
    const QByteArray fragmentSource = ff.readAll();

    return load(vertexSource, fragmentSource);
}

bool GLShader::compile(GLuint program, GLenum shaderType, const QByteArray &source) const
{
    GLuint shader = glCreateShader(shaderType);

    // Prepare the source code
    QByteArray ba;
#ifdef KWIN_HAVE_OPENGLES
    ba.append("#ifdef GL_ES\nprecision highp float;\n#endif\n");
#endif
    ba.append(source);

    const char* src = ba.constData();
    glShaderSource(shader, 1, &src, NULL);

    // Compile the shader
    glCompileShader(shader);

    // Get the shader info log
    int maxLength, length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetShaderInfoLog(shader, maxLength, &length, log.data());

    // Check the status
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == 0) {
        const char *typeName = (shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment");
        kError(1212) << "Failed to compile" << typeName << "shader:" << endl << log << endl;
    } else if (length > 0)
        kDebug(1212) << "Shader compile log:" << log;

    if (status != 0)
        glAttachShader(program, shader);

    glDeleteShader(shader);
    return status != 0;
}

bool GLShader::load(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    // Make sure shaders are actually supported
    if (!GLPlatform::instance()->supports(GLSL) || GLPlatform::instance()->supports(LimitedGLSL)) {
        kError(1212) << "Shaders are not supported";
        return false;
    }

    // Create the shader program
    mProgram = glCreateProgram();

    // Compile the vertex shader
    if (!vertexSource.isEmpty()) {
        bool success = compile(mProgram, GL_VERTEX_SHADER, vertexSource);

        if (!success) {
            glDeleteProgram(mProgram);
            mProgram = 0;
            return false;
        }
    }

    // Compile the fragment shader
    if (!fragmentSource.isEmpty()) {
        bool success = compile(mProgram, GL_FRAGMENT_SHADER, fragmentSource);

        if (!success) {
            glDeleteProgram(mProgram);
            mProgram = 0;
            return false;
        }
    }

    glLinkProgram(mProgram);

    // Get the program info log
    int maxLength, length;
    glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetProgramInfoLog(mProgram, maxLength, &length, log.data());

    // Make sure the program linked successfully
    int status;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);

    if (status == 0) {
        kError(1212) << "Failed to link shader:" << endl << log << endl;
        glDeleteProgram(mProgram);
        mProgram = 0;
        return false;
    } else if (length > 0)
        kDebug(1212) << "Shader link log:" << log;

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

void GLShader::resolveLocations()
{
    if (mLocationsResolved)
        return;

    mMatrixLocation[TextureMatrix]        = uniformLocation("textureMatrix");
    mMatrixLocation[ProjectionMatrix]     = uniformLocation("projection");
    mMatrixLocation[ModelViewMatrix]      = uniformLocation("modelview");
    mMatrixLocation[WindowTransformation] = uniformLocation("windowTransformation");
    mMatrixLocation[ScreenTransformation] = uniformLocation("screenTransformation");

    mVec2Location[Offset] = uniformLocation("offset");

    mVec4Location[ModulationConstant] = uniformLocation("modulation");

    mFloatLocation[Saturation]    = uniformLocation("saturation");
    mFloatLocation[TextureWidth]  = uniformLocation("textureWidth");
    mFloatLocation[TextureHeight] = uniformLocation("textureHeight");

    mIntLocation[AlphaToOne] = uniformLocation("u_forceAlpha");

    mLocationsResolved = true;
}

int GLShader::uniformLocation(const char *name)
{
    const int location = glGetUniformLocation(mProgram, name);
    return location;
}

bool GLShader::setUniform(GLShader::MatrixUniform uniform, const QMatrix4x4 &matrix)
{
    resolveLocations();
    return setUniform(mMatrixLocation[uniform], matrix);
}

bool GLShader::setUniform(GLShader::Vec2Uniform uniform, const QVector2D &value)
{
    resolveLocations();
    return setUniform(mVec2Location[uniform], value);
}

bool GLShader::setUniform(GLShader::Vec4Uniform uniform, const QVector4D &value)
{
    resolveLocations();
    return setUniform(mVec4Location[uniform], value);
}

bool GLShader::setUniform(GLShader::FloatUniform uniform, float value)
{
    resolveLocations();
    return setUniform(mFloatLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::IntUniform uniform, int value)
{
    resolveLocations();
    return setUniform(mIntLocation[uniform], value);
}

bool GLShader::setUniform(const char *name, float value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, int value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector2D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector3D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector4D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QMatrix4x4& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QColor& color)
{
    const int location = uniformLocation(name);
    return setUniform(location, color);
}

bool GLShader::setUniform(int location, float value)
{
    if (location >= 0) {
        glUniform1f(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, int value)
{
    if (location >= 0) {
        glUniform1i(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector2D &value)
{
    if (location >= 0) {
        glUniform2fv(location, 1, (const GLfloat*)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector3D &value)
{
    if (location >= 0) {
        glUniform3fv(location, 1, (const GLfloat*)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector4D &value)
{
    if (location >= 0) {
        glUniform4fv(location, 1, (const GLfloat*)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QMatrix4x4 &value)
{
    if (location >= 0) {
        GLfloat m[16];
        const qreal *data = value.constData();
        // i is column, j is row for m
        for (int i = 0; i < 16; ++i) {
            m[i] = data[i];
        }
        glUniformMatrix4fv(location, 1, GL_FALSE, m);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QColor &color)
{
    if (location >= 0) {
        glUniform4f(location, color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }
    return (location >= 0);
}

int GLShader::attributeLocation(const char* name)
{
    int location = glGetAttribLocation(mProgram, name);
    return location;
}

bool GLShader::setAttribute(const char* name, float value)
{
    int location = attributeLocation(name);
    if (location >= 0) {
        glVertexAttrib1f(location, value);
    }
    return (location >= 0);
}

void GLShader::setTextureHeight(float height)
{
    mTextureHeight = height;
}

void GLShader::setTextureWidth(float width)
{
    mTextureWidth = width;
}

float GLShader::textureHeight()
{
    return mTextureHeight;
}

float GLShader::textureWidth()
{
    return mTextureWidth;
}

QMatrix4x4 GLShader::getUniformMatrix4x4(const char* name)
{
    int location = uniformLocation(name);
    if (location >= 0) {
        GLfloat m[16];
        glGetUniformfv(mProgram, location, m);
        QMatrix4x4 matrix(m[0], m[4], m[8],  m[12],
                          m[1], m[5], m[9],  m[13],
                          m[2], m[6], m[10], m[14],
                          m[3], m[7], m[11], m[15]);
        matrix.optimize();
        return matrix;
    } else {
        return QMatrix4x4();
    }
}

//****************************************
// ShaderManager
//****************************************
ShaderManager *ShaderManager::s_shaderManager = NULL;

ShaderManager *ShaderManager::instance()
{
    if (!s_shaderManager) {
        s_shaderManager = new ShaderManager();
    }
    return s_shaderManager;
}

void ShaderManager::cleanup()
{
    delete s_shaderManager;
    s_shaderManager = NULL;
}

ShaderManager::ShaderManager()
    : m_orthoShader(NULL)
    , m_genericShader(NULL)
    , m_colorShader(NULL)
    , m_inited(false)
    , m_valid(false)
{
    initShaders();
    m_inited = true;
}

ShaderManager::~ShaderManager()
{
    while (!m_boundShaders.isEmpty()) {
        popShader();
    }
    delete m_orthoShader;
    delete m_genericShader;
    delete m_colorShader;
}

GLShader *ShaderManager::getBoundShader() const
{
    if (m_boundShaders.isEmpty()) {
        return NULL;
    } else {
        return m_boundShaders.top();
    }
}

bool ShaderManager::isShaderBound() const
{
    return !m_boundShaders.isEmpty();
}

bool ShaderManager::isValid() const
{
    return m_valid;
}

GLShader *ShaderManager::pushShader(ShaderType type, bool reset)
{
    if (m_inited && !m_valid) {
        return NULL;
    }
    GLShader *shader;
    switch(type) {
    case SimpleShader:
        shader = m_orthoShader;
        break;
    case GenericShader:
        shader = m_genericShader;
        break;
    case ColorShader:
        shader = m_colorShader;
        break;
    default:
        return NULL;
    }

    pushShader(shader);
    if (reset) {
        resetShader(type);
    }

    return shader;
}

void ShaderManager::pushShader(GLShader *shader)
{
    // only bind shader if it is not already bound
    if (shader != getBoundShader()) {
        shader->bind();
    }
    m_boundShaders.push(shader);
}

void ShaderManager::popShader()
{
    if (m_boundShaders.isEmpty()) {
        return;
    }
    GLShader *shader = m_boundShaders.pop();
    if (m_boundShaders.isEmpty()) {
        // no more shader bound - unbind
        shader->unbind();
    } else if (shader != m_boundShaders.top()) {
        // only rebind if a different shader is on top of stack
        m_boundShaders.top()->bind();
    }
}

GLShader *ShaderManager::loadFragmentShader(ShaderType vertex, const QString &fragmentFile)
{
    QString vertexShader;
    switch(vertex) {
    case SimpleShader:
        vertexShader = ":/resources/scene-vertex.glsl";
        break;
    case GenericShader:
        vertexShader = ":/resources/scene-generic-vertex.glsl";
        break;
    case ColorShader:
        vertexShader = ":/resources/scene-color-vertex.glsl";
        break;
    }
    GLShader *shader = new GLShader(vertexShader, fragmentFile);
    if (shader->isValid()) {
        pushShader(shader);
        resetShader(vertex);
        popShader();
    }
    return shader;
}

GLShader *ShaderManager::loadVertexShader(ShaderType fragment, const QString &vertexFile)
{
    QString fragmentShader;
    switch(fragment) {
        // Simple and Generic Shader use same fragment Shader
    case SimpleShader:
    case GenericShader:
        fragmentShader = ":/resources/scene-fragment.glsl";
        break;
    case ColorShader:
        fragmentShader = ":/resources/scene-color-fragment.glsl";
        break;
    }
    GLShader *shader = new GLShader(vertexFile, fragmentShader);
    if (shader->isValid()) {
        pushShader(shader);
        resetShader(fragment);
        popShader();
    }
    return shader;
}

GLShader *ShaderManager::loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    GLShader *shader = new GLShader();
    shader->load(vertexSource, fragmentSource);
    return shader;
}

void ShaderManager::initShaders()
{
    if (legacyGl) {
        kDebug(1212) << "OpenGL Shaders disabled by config option";
        return;
    }
    m_orthoShader = new GLShader(":/resources/scene-vertex.glsl", ":/resources/scene-fragment.glsl");
    if (m_orthoShader->isValid()) {
        pushShader(SimpleShader, true);
        popShader();
        kDebug(1212) << "Ortho Shader is valid";
    } else {
        delete m_orthoShader;
        m_orthoShader = NULL;
        kDebug(1212) << "Orho Shader is not valid";
        return;
    }
    m_genericShader = new GLShader(":/resources/scene-generic-vertex.glsl", ":/resources/scene-fragment.glsl");
    if (m_genericShader->isValid()) {
        pushShader(GenericShader, true);
        popShader();
        kDebug(1212) << "Generic Shader is valid";
    } else {
        delete m_genericShader;
        m_genericShader = NULL;
        delete m_orthoShader;
        m_orthoShader = NULL;
        kDebug(1212) << "Generic Shader is not valid";
        return;
    }
    m_colorShader = new GLShader(":/resources/scene-color-vertex.glsl", ":/resources/scene-color-fragment.glsl");
    if (m_colorShader->isValid()) {
        pushShader(ColorShader, true);
        popShader();
        kDebug(1212) << "Color Shader is valid";
    } else {
        delete m_genericShader;
        m_genericShader = NULL;
        delete m_orthoShader;
        m_orthoShader = NULL;
        delete m_colorShader;
        m_colorShader = NULL;
        kDebug(1212) << "Color Scene Shader is not valid";
        return;
    }
    m_valid = true;
}

void ShaderManager::resetShader(ShaderType type)
{
    // resetShader is either called from init or from push, we know that a built-in shader is bound
    const QMatrix4x4 identity;

    QMatrix4x4 projection;
    QMatrix4x4 modelView;

    GLShader *shader = getBoundShader();

    switch(type) {
    case SimpleShader:
        projection.ortho(0, displayWidth(), displayHeight(), 0, 0, 65535);
        break;

    case GenericShader: {
        // Set up the projection matrix
        float fovy   = 60.0f;
        float aspect = 1.0f;
        float zNear  = 0.1f;
        float zFar   = 100.0f;
        float ymax   = zNear * tan(fovy  * M_PI / 360.0f);
        float ymin   = -ymax;
        float xmin   =  ymin * aspect;
        float xmax   = ymax * aspect;
        projection.frustum(xmin, xmax, ymin, ymax, zNear, zFar);

        // Set up the model-view matrix
        float scaleFactor = 1.1 * tan(fovy * M_PI / 360.0f) / ymax;
        modelView.translate(xmin * scaleFactor, ymax * scaleFactor, -1.1);
        modelView.scale((xmax - xmin)*scaleFactor / displayWidth(), -(ymax - ymin)*scaleFactor / displayHeight(), 0.001);
        break;
    }

    case ColorShader:
        projection.ortho(0, displayWidth(), displayHeight(), 0, 0, 65535);
        shader->setUniform("geometryColor", QVector4D(0, 0, 0, 1));
        break;
    }

    //shader->setUniform("debug", 0);
    shader->setUniform("sampler", 0);

    shader->setUniform(GLShader::ProjectionMatrix,     projection);
    shader->setUniform(GLShader::ModelViewMatrix,      modelView);
    shader->setUniform(GLShader::ScreenTransformation, identity);
    shader->setUniform(GLShader::WindowTransformation, identity);

    shader->setUniform(GLShader::Offset, QVector2D(0, 0));
    shader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));

    shader->setUniform(GLShader::Saturation, 1.0f);
    shader->setUniform(GLShader::AlphaToOne, 0);

    // TODO: has to become textureSize
    shader->setUniform(GLShader::TextureWidth,  1.0f);
    shader->setUniform(GLShader::TextureHeight, 1.0f);
}

/***  GLRenderTarget  ***/
bool GLRenderTarget::sSupported = false;
QStack<GLRenderTarget*> GLRenderTarget::s_renderTargets = QStack<GLRenderTarget*>();

void GLRenderTarget::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    sSupported = true;
#else
    sSupported = hasGLExtension("GL_EXT_framebuffer_object") && glFramebufferTexture2D;
#endif
}

bool GLRenderTarget::isRenderTargetBound()
{
    return !s_renderTargets.isEmpty();
}

void GLRenderTarget::pushRenderTarget(GLRenderTarget* target)
{
    target->enable();
    s_renderTargets.push(target);
}

GLRenderTarget* GLRenderTarget::popRenderTarget()
{
    GLRenderTarget* ret = s_renderTargets.pop();
    ret->disable();
    if (!s_renderTargets.isEmpty())
        s_renderTargets.top()->enable();
    return ret;
}

GLRenderTarget::GLRenderTarget(GLTexture* color)
{
    // Reset variables
    mValid = false;

    mTexture = color;

    // Make sure FBO is supported
    if (sSupported && mTexture && !mTexture->isNull()) {
        initFBO();
    } else
        kError(1212) << "Render targets aren't supported!" << endl;
}

GLRenderTarget::~GLRenderTarget()
{
    if (mValid) {
        glDeleteFramebuffers(1, &mFramebuffer);
    }
}

bool GLRenderTarget::enable()
{
    if (!valid()) {
        kError(1212) << "Can't enable invalid render target!" << endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    mTexture->setDirty();

    return true;
}

bool GLRenderTarget::disable()
{
    if (!valid()) {
        kError(1212) << "Can't disable invalid render target!" << endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mTexture->setDirty();

    return true;
}

static QString formatFramebufferStatus(GLenum status)
{
    switch(status) {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        // An attachment is the wrong type / is invalid / has 0 width or height
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        // There are no images attached to the framebuffer
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        // A format or the combination of formats of the attachments is unsupported
        return "GL_FRAMEBUFFER_UNSUPPORTED";
#ifndef KWIN_HAVE_OPENGLES
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
        // Not all attached images have the same width and height
        return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT";
    case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
        // The color attachments don't have the same format
        return "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
        // The attachments don't have the same number of samples
        return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
        // The draw buffer is missing
        return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
        // The read buffer is missing
        return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
#endif
    default:
        return "Unknown (0x" + QString::number(status, 16) + ')';
    }
}

void GLRenderTarget::initFBO()
{
#if DEBUG_GLRENDERTARGET
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        kError(1212) << "Error status when entering GLRenderTarget::initFBO: " << formatGLError(err);
#endif

    glGenFramebuffers(1, &mFramebuffer);

#if DEBUG_GLRENDERTARGET
    if ((err = glGetError()) != GL_NO_ERROR) {
        kError(1212) << "glGenFramebuffers failed: " << formatGLError(err);
        return;
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

#if DEBUG_GLRENDERTARGET
    if ((err = glGetError()) != GL_NO_ERROR) {
        kError(1212) << "glBindFramebuffer failed: " << formatGLError(err);
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }
#endif

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           mTexture->target(), mTexture->texture(), 0);

#if DEBUG_GLRENDERTARGET
    if ((err = glGetError()) != GL_NO_ERROR) {
        kError(1212) << "glFramebufferTexture2D failed: " << formatGLError(err);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }
#endif

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // We have an incomplete framebuffer, consider it invalid
        if (status == 0)
            kError(1212) << "glCheckFramebufferStatus failed: " << formatGLError(glGetError());
        else
            kError(1212) << "Invalid framebuffer status: " << formatFramebufferStatus(status);
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }

    mValid = true;
}


//*********************************
// GLVertexBufferPrivate
//*********************************
class GLVertexBufferPrivate
{
public:
    GLVertexBufferPrivate(GLVertexBuffer::UsageHint usageHint)
        : hint(usageHint)
        , numberVertices(0)
        , dimension(2)
        , useColor(false)
        , useTexCoords(true)
        , color(0, 0, 0, 255) {
        if (GLVertexBufferPrivate::supported) {
            glGenBuffers(2, buffers);
        }
    }
    ~GLVertexBufferPrivate() {
        if (GLVertexBufferPrivate::supported) {
            glDeleteBuffers(2, buffers);
        }
    }
    GLVertexBuffer::UsageHint hint;
    GLuint buffers[2];
    int numberVertices;
    int dimension;
    static bool supported;
    static GLVertexBuffer *streamingBuffer;
    QVector<float> legacyVertices;
    QVector<float> legacyTexCoords;
    bool useColor;
    bool useTexCoords;
    QColor color;

    //! VBO is not supported
    void legacyPainting(QRegion region, GLenum primitiveMode);
    //! VBO and shaders are both supported
    void corePainting(const QRegion& region, GLenum primitiveMode);
    //! VBO is supported, but shaders are not supported
    void fallbackPainting(const QRegion& region, GLenum primitiveMode);
};
bool GLVertexBufferPrivate::supported = false;
GLVertexBuffer *GLVertexBufferPrivate::streamingBuffer = NULL;

void GLVertexBufferPrivate::legacyPainting(QRegion region, GLenum primitiveMode)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(region)
    Q_UNUSED(primitiveMode)
#else
    // Enable arrays
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(dimension, GL_FLOAT, 0, legacyVertices.constData());
    if (!legacyTexCoords.isEmpty()) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, legacyTexCoords.constData());
    }

    if (useColor) {
        glColor4f(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }

    // Clip using scissoring
    if (region != infiniteRegion()) {
        PaintClipper pc(region);
        for (PaintClipper::Iterator iterator; !iterator.isDone(); iterator.next()) {
            glDrawArrays(primitiveMode, 0, numberVertices);
        }
    } else {
        glDrawArrays(primitiveMode, 0, numberVertices);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    if (!legacyTexCoords.isEmpty()) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
#endif
}

void GLVertexBufferPrivate::corePainting(const QRegion& region, GLenum primitiveMode)
{
    GLShader *shader = ShaderManager::instance()->getBoundShader();
    GLint vertexAttrib = shader->attributeLocation("vertex");
    GLint texAttrib = shader->attributeLocation("texCoord");

    glEnableVertexAttribArray(vertexAttrib);
    if (useTexCoords) {
        glEnableVertexAttribArray(texAttrib);
    }

    if (useColor) {
        shader->setUniform("geometryColor", color);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers[ 0 ]);
    glVertexAttribPointer(vertexAttrib, dimension, GL_FLOAT, GL_FALSE, 0, 0);

    if (texAttrib != -1 && useTexCoords) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[ 1 ]);
        glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // Clip using scissoring
    if (region != infiniteRegion()) {
        PaintClipper pc(region);
        for (PaintClipper::Iterator iterator; !iterator.isDone(); iterator.next()) {
            glDrawArrays(primitiveMode, 0, numberVertices);
        }
    } else {
        glDrawArrays(primitiveMode, 0, numberVertices);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (useTexCoords) {
        glDisableVertexAttribArray(texAttrib);
    }
    glDisableVertexAttribArray(vertexAttrib);
}

void GLVertexBufferPrivate::fallbackPainting(const QRegion& region, GLenum primitiveMode)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(region)
    Q_UNUSED(primitiveMode)
#else
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[ 0 ]);
    glVertexPointer(dimension, GL_FLOAT, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, buffers[ 1 ]);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);

    if (useColor) {
        glColor4f(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }

    // Clip using scissoring
    if (region != infiniteRegion()) {
        PaintClipper pc(region);
        for (PaintClipper::Iterator iterator; !iterator.isDone(); iterator.next()) {
            glDrawArrays(primitiveMode, 0, numberVertices);
        }
    } else {
        glDrawArrays(primitiveMode, 0, numberVertices);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
}

//*********************************
// GLVertexBuffer
//*********************************
GLVertexBuffer::GLVertexBuffer(UsageHint hint)
    : d(new GLVertexBufferPrivate(hint))
{
}

GLVertexBuffer::~GLVertexBuffer()
{
    delete d;
}

void GLVertexBuffer::setData(int numberVertices, int dim, const float* vertices, const float* texcoords)
{
    d->numberVertices = numberVertices;
    d->dimension = dim;
    d->useTexCoords = (texcoords != NULL);
    if (!GLVertexBufferPrivate::supported) {
        // legacy data
        d->legacyVertices.clear();
        d->legacyVertices.reserve(numberVertices * dim);
        for (int i = 0; i < numberVertices * dim; ++i) {
            d->legacyVertices << vertices[i];
        }
        d->legacyTexCoords.clear();
        if (d->useTexCoords) {
            d->legacyTexCoords.reserve(numberVertices * 2);
            for (int i = 0; i < numberVertices * 2; ++i) {
                d->legacyTexCoords << texcoords[i];
            }
        }
        return;
    }
    GLenum hint;
    switch(d->hint) {
    case Dynamic:
        hint = GL_DYNAMIC_DRAW;
        break;
    case Static:
        hint = GL_STATIC_DRAW;
        break;
    case Stream:
        hint = GL_STREAM_DRAW;
        break;
    default:
        // just to make the compiler happy
        hint = GL_STREAM_DRAW;
        break;
    }
    glBindBuffer(GL_ARRAY_BUFFER, d->buffers[ 0 ]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*numberVertices * d->dimension, vertices, hint);

    if (d->useTexCoords) {
        glBindBuffer(GL_ARRAY_BUFFER, d->buffers[ 1 ]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*numberVertices * 2, texcoords, hint);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLVertexBuffer::render(GLenum primitiveMode)
{
    render(infiniteRegion(), primitiveMode);
}

void GLVertexBuffer::render(const QRegion& region, GLenum primitiveMode)
{
    if (!GLVertexBufferPrivate::supported) {
        d->legacyPainting(region, primitiveMode);
    } else if (ShaderManager::instance()->isShaderBound()) {
        d->corePainting(region, primitiveMode);
    } else {
        d->fallbackPainting(region, primitiveMode);
    }
}

bool GLVertexBuffer::isSupported()
{
    return GLVertexBufferPrivate::supported;
}

bool GLVertexBuffer::isUseColor() const
{
    return d->useColor;
}

void GLVertexBuffer::setUseColor(bool enable)
{
    d->useColor = enable;
}

void GLVertexBuffer::setColor(const QColor& color, bool enable)
{
    d->useColor = enable;
    d->color = color;
}

void GLVertexBuffer::reset()
{
    d->useColor       = false;
    d->color          = QColor(0, 0, 0, 255);
    d->numberVertices = 0;
    d->dimension      = 2;
    d->useTexCoords   = true;
}

void GLVertexBuffer::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    GLVertexBufferPrivate::supported = true;
#else
    GLVertexBufferPrivate::supported = hasGLExtension("GL_ARB_vertex_buffer_object");
#endif
    GLVertexBufferPrivate::streamingBuffer = new GLVertexBuffer(GLVertexBuffer::Stream);
}

GLVertexBuffer *GLVertexBuffer::streamingBuffer()
{
    return GLVertexBufferPrivate::streamingBuffer;
}

} // namespace

#endif
