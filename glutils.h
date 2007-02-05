/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GLUTILS_H
#define KWIN_GLUTILS_H


#include "utils.h"

#include <QStringList>

#include <GL/gl.h>
#include <GL/glx.h>

#include "glutils_funcs.h"


template< class K, class V > class QHash;


namespace KWinInternal
{


// Initializes GLX function pointers
void initGLX();
// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
void initGL();


// Number of supported texture units
extern int glTextureUnitsCount;


bool hasGLVersion(int major, int minor, int release = 0);
bool hasGLXVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool hasGLExtension(const QString& extension);

inline bool isPowerOfTwo( int x ) { return (( x & ( x - 1 )) == 0 ); }


class GLShader
    {
    public:
        GLShader(const QString& vertexfile, const QString& fragmentfile);

        bool isValid() const  { return mValid; }
        void bind();
        void unbind();

        int uniformLocation(const QString& name);
        bool setUniform(const QString& name, float value);
        bool setUniform(const QString& name, int value);
        int attributeLocation(const QString& name);
        bool setAttribute(const QString& name, float value);


    protected:
        bool loadFromFiles(const QString& vertexfile, const QString& fragmentfile);
        bool load(const QString& vertexsource, const QString& fragmentsource);


    private:
        unsigned int mProgram;
        bool mValid;
        QHash< QString, int >* mVariableLocations;
    };

} // namespace

#endif
