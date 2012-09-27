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

// need to call GLTexturePrivate::initStatic()
#include "kwingltexture_p.h"

#include "kwinglcolorcorrection.h"
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
#include <QVarLengthArray>

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
#ifdef KWIN_HAVE_EGL
    EGLDisplay dpy = eglGetCurrentDisplay();
    int major, minor;
    eglInitialize(dpy, &major, &minor);
    eglVersion = MAKE_GL_VERSION(major, minor, 0);
    eglExtension = QString((const char*)eglQueryString(dpy, EGL_EXTENSIONS)).split(' ');

    eglResolveFunctions();
#endif
}

void initGL(OpenGLPlatformInterface platformInterface)
{
    // Get OpenGL version
    QString glversionstring = QString((const char*)glGetString(GL_VERSION));
    QStringList glversioninfo = glversionstring.left(glversionstring.indexOf(' ')).split('.');
    while (glversioninfo.count() < 3)
        glversioninfo << "0";

#ifndef KWIN_HAVE_OPENGLES
    glVersion = MAKE_GL_VERSION(glversioninfo[0].toInt(), glversioninfo[1].toInt(), glversioninfo[2].toInt());

    // Get list of supported OpenGL extensions
    if (hasGLVersion(3, 0)) {
        PFNGLGETSTRINGIPROC glGetStringi;

#ifdef KWIN_HAVE_EGL
        if (platformInterface == EglPlatformInterface)
            glGetStringi = (PFNGLGETSTRINGIPROC) eglGetProcAddress("glGetStringi");
        else
#endif
            glGetStringi = (PFNGLGETSTRINGIPROC) glXGetProcAddress((const GLubyte *) "glGetStringi");

        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const char *name = (const char *) glGetStringi(GL_EXTENSIONS, i);
            glExtensions << QString(name);
        }
    } else
#endif
        glExtensions = QString((const char*)glGetString(GL_EXTENSIONS)).split(' ');

    // handle OpenGL extensions functions
    glResolveFunctions(platformInterface);

    GLTexturePrivate::initStatic();
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
    bool hasError = false;
    while (err != GL_NO_ERROR) {
        kWarning(1212) << "GL error (" << txt << "): " << formatGLError(err);
        hasError = true;
        err = glGetError();
    }
    return hasError;
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
#ifdef KWIN_HAVE_OPENGL_1
    if (ShaderManager::instance()->isValid()) {
        return;
    }
    glPushMatrix();
#endif
}

void pushMatrix(const QMatrix4x4 &matrix)
{
#ifndef KWIN_HAVE_OPENGL_1
    Q_UNUSED(matrix)
#else
    if (ShaderManager::instance()->isValid()) {
        return;
    }
    glPushMatrix();
    multiplyMatrix(matrix);
#endif
}

void multiplyMatrix(const QMatrix4x4 &matrix)
{
#ifndef KWIN_HAVE_OPENGL_1
    Q_UNUSED(matrix)
#else
    if (ShaderManager::instance()->isValid()) {
        return;
    }
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
#ifndef KWIN_HAVE_OPENGL_1
    Q_UNUSED(matrix)
#else
    if (ShaderManager::instance()->isValid()) {
        return;
    }
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
#ifdef KWIN_HAVE_OPENGL_1
    if (ShaderManager::instance()->isValid()) {
        return;
    }
    glPopMatrix();
#endif
}


//****************************************
// GLShader
//****************************************

bool GLShader::sColorCorrect = false;

GLShader::GLShader(unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
}

GLShader::GLShader(const QString& vertexfile, const QString& fragmentfile, unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
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

bool GLShader::link()
{
    // Be optimistic
    mValid = true;

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
        mValid = false;
    } else if (length > 0) {
        kDebug(1212) << "Shader link log:" << log;
    }

    return mValid;
}

const QByteArray GLShader::prepareSource(GLenum shaderType, const QByteArray &source) const
{
    // Prepare the source code
    QByteArray ba;
#ifdef KWIN_HAVE_OPENGLES
    if (GLPlatform::instance()->glslVersion() < kVersionNumber(3, 0)) {
        ba.append("precision highp float;\n");
    }
#endif
    if (ShaderManager::instance()->isShaderDebug()) {
        ba.append("#define KWIN_SHADER_DEBUG 1\n");
    }
    ba.append(source);
#ifdef KWIN_HAVE_OPENGLES
    if (GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0)) {
        ba.replace("#version 140", "#version 300 es\n\nprecision highp float;\n");
    }
#endif

    // Inject color correction code for fragment shaders, if possible
    if (shaderType == GL_FRAGMENT_SHADER && sColorCorrect)
        ba = ColorCorrection::prepareFragmentShader(ba);

    return ba;
}

bool GLShader::compile(GLuint program, GLenum shaderType, const QByteArray &source) const
{
    GLuint shader = glCreateShader(shaderType);

    QByteArray preparedSource = prepareSource(shaderType, source);
    const char* src = preparedSource.constData();
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
#ifndef KWIN_HAVE_OPENGLES
    // Make sure shaders are actually supported
    if (!GLPlatform::instance()->supports(GLSL) || GLPlatform::instance()->supports(LimitedNPOT)) {
        kError(1212) << "Shaders are not supported";
        return false;
    }
#endif

    mValid = false;

    // Compile the vertex shader
    if (!vertexSource.isEmpty()) {
        bool success = compile(mProgram, GL_VERTEX_SHADER, vertexSource);

        if (!success)
            return false;
    }

    // Compile the fragment shader
    if (!fragmentSource.isEmpty()) {
        bool success = compile(mProgram, GL_FRAGMENT_SHADER, fragmentSource);

        if (!success)
            return false;
    }

    if (mExplicitLinking)
        return true;

    // link() sets mValid
    return link();
}

void GLShader::bindAttributeLocation(const char *name, int index)
{
    glBindAttribLocation(mProgram, index, name);
}

void GLShader::bindFragDataLocation(const char *name, int index)
{
#ifndef KWIN_HAVE_OPENGLES
    if (glBindFragDataLocation)
        glBindFragDataLocation(mProgram, index, name);
#else
    Q_UNUSED(name)
    Q_UNUSED(index)
#endif
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

    mColorLocation[Color] = uniformLocation("geometryColor");

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

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QVector4D &value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QColor &value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
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

QMatrix4x4 GLShader::getUniformMatrix4x4(const char* name)
{
    int location = uniformLocation(name);
    if (location >= 0) {
        GLfloat m[16];
        glGetnUniformfv(mProgram, location, sizeof(m), m);
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

enum VertexAttribute {
    VA_Position = 0,
    VA_TexCoord = 1
};

ShaderManager *ShaderManager::instance()
{
    if (!s_shaderManager) {
        s_shaderManager = new ShaderManager();
        s_shaderManager->initShaders();
        s_shaderManager->m_inited = true;
    }
    return s_shaderManager;
}

void ShaderManager::disable()
{
    // for safety do a Cleanup first
    ShaderManager::cleanup();

    // create a new ShaderManager and set it to inited without calling init
    // that will ensure that the ShaderManager is not valid
    s_shaderManager = new ShaderManager();
    s_shaderManager->m_inited = true;
}

void ShaderManager::cleanup()
{
    delete s_shaderManager;
    s_shaderManager = NULL;
}

ShaderManager::ShaderManager()
    : m_inited(false)
    , m_valid(false)
{
    for (int i = 0; i < 3; i++)
       m_shader[i] = 0;

    m_debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;
}

ShaderManager::~ShaderManager()
{
    while (!m_boundShaders.isEmpty()) {
        popShader();
    }

    for (int i = 0; i < 3; i++)
        delete m_shader[i];
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

bool ShaderManager::isShaderDebug() const
{
    return m_debug;
}

GLShader *ShaderManager::pushShader(ShaderType type, bool reset)
{
    if (m_inited && !m_valid) {
        return NULL;
    }

    pushShader(m_shader[type]);
    if (reset) {
        resetShader(type);
    }

    return m_shader[type];
}

void ShaderManager::resetAllShaders()
{
    if (!m_inited || !m_valid) {
        return;
    }

    for (int i = 0; i < 3; i++) {
        pushShader(ShaderType(i), true);
        popShader();
    }
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

void ShaderManager::bindFragDataLocations(GLShader *shader)
{
    shader->bindFragDataLocation("fragColor", 0);
}

void ShaderManager::bindAttributeLocations(GLShader *shader) const
{
    shader->bindAttributeLocation("vertex",   VA_Position);
    shader->bindAttributeLocation("texCoord", VA_TexCoord);
}

GLShader *ShaderManager::loadFragmentShader(ShaderType vertex, const QString &fragmentFile)
{
    const char *vertexFile[] = {
        "scene-vertex.glsl",
        "scene-generic-vertex.glsl",
        "scene-color-vertex.glsl"
    };

    GLShader *shader = new GLShader(m_shaderDir + vertexFile[vertex], fragmentFile, GLShader::ExplicitLinking);
    bindAttributeLocations(shader);
    bindFragDataLocations(shader);
    shader->link();

    if (shader->isValid()) {
        pushShader(shader);
        resetShader(vertex);
        popShader();
    }

    return shader;
}

GLShader *ShaderManager::loadVertexShader(ShaderType fragment, const QString &vertexFile)
{
    // The Simple and Generic shaders use same fragment shader
    const char *fragmentFile[] = {
        "scene-fragment.glsl",
        "scene-fragment.glsl",
        "scene-color-fragment.glsl"
    };

    GLShader *shader = new GLShader(vertexFile, m_shaderDir + fragmentFile[fragment], GLShader::ExplicitLinking);
    bindAttributeLocations(shader);
    bindFragDataLocations(shader);
    shader->link();

    if (shader->isValid()) {
        pushShader(shader);
        resetShader(fragment);
        popShader();
    }

    return shader;
}

GLShader *ShaderManager::loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    GLShader *shader = new GLShader(GLShader::ExplicitLinking);
    shader->load(vertexSource, fragmentSource);
    bindAttributeLocations(shader);
    bindFragDataLocations(shader);
    shader->link();
    return shader;
}

void ShaderManager::initShaders()
{
    const char *vertexFile[] = {
        "scene-vertex.glsl",
        "scene-generic-vertex.glsl",
        "scene-color-vertex.glsl",
    };

    const char *fragmentFile[] = {
        "scene-fragment.glsl",
        "scene-fragment.glsl",
        "scene-color-fragment.glsl",
    };

#ifdef KWIN_HAVE_OPENGLES
    const qint64 coreVersionNumber = kVersionNumber(3, 0);
#else
    const qint64 coreVersionNumber = kVersionNumber(1, 40);
#endif
    if (GLPlatform::instance()->glslVersion() >= coreVersionNumber)
        m_shaderDir = ":/resources/shaders/1.40/";
    else
        m_shaderDir = ":/resources/shaders/1.10/";

    // Be optimistic
    m_valid = true;

    for (int i = 0; i < 3; i++) {
        m_shader[i] = new GLShader(m_shaderDir + vertexFile[i], m_shaderDir + fragmentFile[i],
                                   GLShader::ExplicitLinking);
        bindAttributeLocations(m_shader[i]);
        bindFragDataLocations(m_shader[i]);
        m_shader[i]->link();

        if (!m_shader[i]->isValid()) {
            m_valid = false;
            break;
        }

        pushShader(m_shader[i]);
        resetShader(ShaderType(i));
        popShader();
    }

    if (!m_valid) {
        for (int i = 0; i < 3; i++) {
            delete m_shader[i];
            m_shader[i] = 0;
        }
    }
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

    shader->setUniform("sampler", 0);

    shader->setUniform(GLShader::ProjectionMatrix,     projection);
    shader->setUniform(GLShader::ModelViewMatrix,      modelView);
    shader->setUniform(GLShader::ScreenTransformation, identity);
    shader->setUniform(GLShader::WindowTransformation, identity);

    shader->setUniform(GLShader::Offset, QVector2D(0, 0));
    shader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));

    shader->setUniform(GLShader::Saturation, 1.0f);
}

/***  GLRenderTarget  ***/
bool GLRenderTarget::sSupported = false;
bool GLRenderTarget::s_blitSupported = false;
QStack<GLRenderTarget*> GLRenderTarget::s_renderTargets = QStack<GLRenderTarget*>();
QSize GLRenderTarget::s_oldViewport;

void GLRenderTarget::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    sSupported = true;
    s_blitSupported = false;
#else
    sSupported = hasGLVersion(3, 0) || hasGLExtension("GL_ARB_framebuffer_object") || hasGLExtension("GL_EXT_framebuffer_object");
    s_blitSupported = hasGLVersion(3, 0) || hasGLExtension("GL_ARB_framebuffer_object") || hasGLExtension("GL_EXT_framebuffer_blit");
#endif
}

bool GLRenderTarget::isRenderTargetBound()
{
    return !s_renderTargets.isEmpty();
}

bool GLRenderTarget::blitSupported()
{
    return s_blitSupported;
}

void GLRenderTarget::pushRenderTarget(GLRenderTarget* target)
{
    if (s_renderTargets.isEmpty()) {
        GLint params[4];
        glGetIntegerv(GL_VIEWPORT, params);
        s_oldViewport = QSize(params[2], params[3]);
    }

    target->enable();
    s_renderTargets.push(target);
}

GLRenderTarget* GLRenderTarget::popRenderTarget()
{
    GLRenderTarget* ret = s_renderTargets.pop();
    ret->disable();
    if (!s_renderTargets.isEmpty()) {
        s_renderTargets.top()->enable();
    } else if (!s_oldViewport.isEmpty()) {
        glViewport (0, 0, s_oldViewport.width(), s_oldViewport.height());
    }
    return ret;
}

GLRenderTarget::GLRenderTarget(const GLTexture& color)
{
    // Reset variables
    mValid = false;

    mTexture = color;

    // Make sure FBO is supported
    if (sSupported && !mTexture.isNull()) {
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
    glViewport(0, 0, mTexture.width(), mTexture.height());
    mTexture.setDirty();

    return true;
}

bool GLRenderTarget::disable()
{
    if (!valid()) {
        kError(1212) << "Can't disable invalid render target!" << endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mTexture.setDirty();

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
                           mTexture.target(), mTexture.texture(), 0);

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

void GLRenderTarget::blitFromFramebuffer(const QRect &source, const QRect &destination, GLenum filter)
{
    if (!GLRenderTarget::blitSupported()) {
        return;
    }
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(source)
    Q_UNUSED(destination)
    Q_UNUSED(filter)
#else
    GLRenderTarget::pushRenderTarget(this);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    const QRect s = source.isNull() ? QRect(0, 0, displayWidth(), displayHeight()) : source;
    const QRect d = destination.isNull() ? QRect(0, 0, mTexture.width(), mTexture.height()) : destination;

    glBlitFramebuffer(s.x(), displayHeight() - s.y() - s.height(), s.x() + s.width(), displayHeight() - s.y(),
                      d.x(), mTexture.height() - d.y() - d.height(), d.x() + d.width(), mTexture.height() - d.y(),
                      GL_COLOR_BUFFER_BIT, filter);
    GLRenderTarget::popRenderTarget();
#endif
}

void GLRenderTarget::attachTexture(const GLTexture& target)
{
    if (!mValid || mTexture.texture() == target.texture()) {
        return;
    }

    pushRenderTarget(this);

    mTexture = target;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           mTexture.target(), mTexture.texture(), 0);

    popRenderTarget();
}


// ------------------------------------------------------------------


class BitRef
{
public:
    BitRef(uint32_t &bitfield, int bit) : m_bitfield(bitfield), m_mask(1 << bit) {}

    void operator = (bool val) {
        if (val)
            m_bitfield |= m_mask;
        else
            m_bitfield &= ~m_mask;
    }

    operator bool () const { return m_bitfield & m_mask; }

private:
    uint32_t &m_bitfield;
    int const m_mask;
};


// ------------------------------------------------------------------


class Bitfield
{
public:
    Bitfield() : m_bitfield(0) {}
    Bitfield(uint32_t bits) : m_bitfield(bits) {}

    void set(int i) { m_bitfield |= (1 << i); }
    void clear(int i) { m_bitfield &= ~(1 << i); }

    BitRef operator [] (int i) { return BitRef(m_bitfield, i); }
    operator uint32_t () const { return m_bitfield; }

private:
    uint32_t m_bitfield;
};


// ------------------------------------------------------------------


class BitfieldIterator
{
public:
    BitfieldIterator(uint32_t bitfield) : m_bitfield(bitfield) {}

    bool hasNext() const { return m_bitfield != 0; }

    int next() {
        const int bit = ffs(m_bitfield) - 1;
        m_bitfield ^= (1 << bit);
        return bit;
    }

private:
    uint32_t m_bitfield;
};



// ------------------------------------------------------------------



struct VertexAttrib
{
    int size;
    GLenum type;
    int offset;
};


//*********************************
// GLVertexBufferPrivate
//*********************************
class GLVertexBufferPrivate
{
public:
    GLVertexBufferPrivate(GLVertexBuffer::UsageHint usageHint)
        : vertexCount(0)
        , useColor(false)
        , color(0, 0, 0, 255)
        , bufferSize(0)
        , nextOffset(0)
        , baseAddress(0)
    {
        if (GLVertexBufferPrivate::supported)
            glGenBuffers(1, &buffer);

        switch(usageHint) {
        case GLVertexBuffer::Dynamic:
            usage = GL_DYNAMIC_DRAW;
            break;
        case GLVertexBuffer::Static:
            usage = GL_STATIC_DRAW;
            break;
        default:
            usage = GL_STREAM_DRAW;
            break;
        }
    }

    ~GLVertexBufferPrivate() {
        if (GLVertexBufferPrivate::supported)
            glDeleteBuffers(1, &buffer);
    }

    void interleaveArrays(float *array, int dim, const float *vertices, const float *texcoords, int count);
    void bindArrays();
    void unbindArrays();
    GLvoid *mapNextFreeRange(size_t size);

    GLuint buffer;
    GLenum usage;
    int stride;
    int vertexCount;
    static bool supported;
    static GLVertexBuffer *streamingBuffer;
    static bool hasMapBufferRange;
    QByteArray dataStore;
    bool useColor;
    QVector4D color;
    size_t bufferSize;
    intptr_t nextOffset;
    intptr_t baseAddress;
    VertexAttrib attrib[2];
    Bitfield enabledArrays;
};

bool GLVertexBufferPrivate::supported = false;
bool GLVertexBufferPrivate::hasMapBufferRange = false;
GLVertexBuffer *GLVertexBufferPrivate::streamingBuffer = NULL;

void GLVertexBufferPrivate::interleaveArrays(float *dst, int dim,
                                             const float *vertices, const float *texcoords,
                                             int count)
{
    if (!texcoords) {
        memcpy((void *) dst, vertices, dim * sizeof(float) * count);
        return;
    }

    switch (dim)
    {
    case 2:
        for (int i = 0; i < count; i++) {
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
        break;

    case 3:
        for (int i = 0; i < count; i++) {
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
        break;

    default:
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < dim; j++)
                *(dst++) = *(vertices++);

            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
    }
}

void GLVertexBufferPrivate::bindArrays()
{
#ifndef KWIN_HAVE_OPENGLES
    if (ShaderManager::instance()->isShaderBound()) {
#endif
        if (useColor) {
            GLShader *shader = ShaderManager::instance()->getBoundShader();
            shader->setUniform(GLShader::Color, color);
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer);

        BitfieldIterator it(enabledArrays);
        while (it.hasNext()) {
            const int index = it.next();
            glVertexAttribPointer(index, attrib[index].size, attrib[index].type, GL_FALSE, stride,
                                 (const GLvoid *) (baseAddress + attrib[index].offset));
            glEnableVertexAttribArray(index);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
#ifndef KWIN_HAVE_OPENGLES
    } else {
        if (GLVertexBufferPrivate::supported)
            glBindBuffer(GL_ARRAY_BUFFER, buffer);

        // FIXME Is there any good reason to not leave this array permanently enabled?
        //       When do we not use it in the GL 1.x path?
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(attrib[VA_Position].size, attrib[VA_Position].type, stride,
                        (const GLvoid *) (baseAddress + attrib[VA_Position].offset));

        if (enabledArrays[VA_TexCoord]) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(attrib[VA_TexCoord].size, attrib[VA_TexCoord].type, stride,
                              (const GLvoid *) (baseAddress + attrib[VA_TexCoord].offset));
        }

        if (useColor)
            glColor4f(color.x(), color.y(), color.z(), color.w());

        if (GLVertexBufferPrivate::supported)
            glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
#endif
}

void GLVertexBufferPrivate::unbindArrays()
{
#ifndef KWIN_HAVE_OPENGLES
    if (ShaderManager::instance()->isShaderBound()) {
#endif
        BitfieldIterator it(enabledArrays);
        while (it.hasNext())
            glDisableVertexAttribArray(it.next());
#ifndef KWIN_HAVE_OPENGLES
    } else {
        // Assume that the conventional arrays were enabled
        glDisableClientState(GL_VERTEX_ARRAY);

        if (enabledArrays[VA_TexCoord])
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
#endif
}

template <typename T>
T align(T value, int bytes)
{
    return (value + bytes - 1) & ~T(bytes - 1);
}

GLvoid *GLVertexBufferPrivate::mapNextFreeRange(size_t size)
{
    GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;

    if ((nextOffset + size) > bufferSize) {
        // Reallocate the data store if it's too small.
        if (size > bufferSize) {
            // Round the size up to 4 Kb for streaming/dynamic buffers.
            const size_t minSize = 32768; // Minimum size for streaming buffers
            const size_t alloc = usage != GL_STATIC_DRAW ? align(qMax(size, minSize), 4096) : size;

            glBufferData(GL_ARRAY_BUFFER, alloc, 0, usage);

            bufferSize = alloc;
        } else {
            access |= GL_MAP_INVALIDATE_BUFFER_BIT;
            access ^= GL_MAP_UNSYNCHRONIZED_BIT;
        }

        nextOffset = 0;
    }

    return glMapBufferRange(GL_ARRAY_BUFFER, nextOffset, size, access);
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

void GLVertexBuffer::setData(int vertexCount, int dim, const float* vertices, const float* texcoords)
{
    d->vertexCount = vertexCount;
    d->stride = (dim + (texcoords ? 2 : 0)) * sizeof(float);

    d->attrib[VA_Position].size = dim;
    d->attrib[VA_Position].type = GL_FLOAT;
    d->attrib[VA_Position].offset = 0;

    d->attrib[VA_TexCoord].size = 2;
    d->attrib[VA_TexCoord].type = GL_FLOAT;
    d->attrib[VA_TexCoord].offset = dim * sizeof(float);

    d->enabledArrays[VA_Position] = true;
    d->enabledArrays[VA_TexCoord] = texcoords != 0;

    size_t size = vertexCount * d->stride;

    if (!GLVertexBufferPrivate::supported) {
        // Legacy data
        if (size_t(d->dataStore.size()) < size)
            d->dataStore.resize(size);

        d->baseAddress = intptr_t(d->dataStore.data());
        d->interleaveArrays((float *) d->baseAddress,
                            dim, vertices, texcoords, vertexCount);
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, d->buffer);

    if (d->hasMapBufferRange) {
        // Upload the data into the next free range in the buffer
        float *map = (float *) d->mapNextFreeRange(size);
        d->interleaveArrays(map, dim, vertices, texcoords, vertexCount);
        glUnmapBuffer(GL_ARRAY_BUFFER);

        d->baseAddress = d->nextOffset;
        d->nextOffset += align(size, 16); // Align to 16 bytes for SSE
    } else {
        // We always reallocate the data store when we can't map the buffer.
        // The rationale is that there will almost always be contention
        // in a glBufferSubData() call.
        if (texcoords) {
            QByteArray array;
            array.resize(size);

            d->interleaveArrays((float *) array.data(), dim, vertices, texcoords, vertexCount);
            glBufferData(GL_ARRAY_BUFFER, size, array.data(), d->usage);
        } else
            glBufferData(GL_ARRAY_BUFFER, size, vertices, d->usage);

        d->baseAddress = 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLVertexBuffer::render(GLenum primitiveMode)
{
    render(infiniteRegion(), primitiveMode, false);
}

void GLVertexBuffer::render(const QRegion& region, GLenum primitiveMode, bool hardwareClipping)
{
    d->bindArrays();

    if (!hardwareClipping) {
        glDrawArrays(primitiveMode, 0, d->vertexCount);
    } else {
        // Clip using scissoring
        foreach (const QRect &r, region.rects()) {
            glScissor(r.x(), displayHeight() - r.y() - r.height(), r.width(), r.height());
            glDrawArrays(primitiveMode, 0, d->vertexCount);
        }
    }

    d->unbindArrays();
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
    d->color = QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

void GLVertexBuffer::reset()
{
    d->useColor       = false;
    d->color          = QVector4D(0, 0, 0, 1);
    d->vertexCount    = 0;
}

void GLVertexBuffer::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    GLVertexBufferPrivate::supported = true;
    GLVertexBufferPrivate::hasMapBufferRange = hasGLExtension("GL_EXT_map_buffer_range");
#else
    GLVertexBufferPrivate::supported = hasGLVersion(1, 5) || hasGLExtension("GL_ARB_vertex_buffer_object");
    GLVertexBufferPrivate::hasMapBufferRange = hasGLVersion(3, 0) || hasGLExtension("GL_ARB_map_buffer_range");
#endif
    GLVertexBufferPrivate::streamingBuffer = new GLVertexBuffer(GLVertexBuffer::Stream);
}

GLVertexBuffer *GLVertexBuffer::streamingBuffer()
{
    return GLVertexBufferPrivate::streamingBuffer;
}

} // namespace
