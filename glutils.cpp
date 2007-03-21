/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "glutils.h"

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


#ifdef HAVE_OPENGL

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
