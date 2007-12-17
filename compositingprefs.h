/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_COMPOSITINGPREFS_H
#define KWIN_COMPOSITINGPREFS_H

#include <QString>
#include <QStringList>

#include "kwinglutils.h"


namespace KWin
{

class CompositingPrefs
{
public:
    CompositingPrefs();
    ~CompositingPrefs();

    class Version : public QStringList
    {
    public:
        Version() : QStringList()  {}
        Version( const QString& str );

        int compare( const Version& v ) const;

        bool operator<( const Version& v ) const  { return ( compare( v ) == -1 ); }
        bool operator<=( const Version& v ) const  { return ( compare( v ) != 1 ); }
        bool operator>( const Version& v ) const  { return ( compare( v ) == 1 ); }
        bool operator>=( const Version& v ) const  { return ( compare( v ) != -1 ); }
    };

    static bool compositingPossible();
    static QString compositingNotPossibleReason();
    bool enableCompositing() const  { return mEnableCompositing; }
    bool enableVSync() const  { return mEnableVSync; }
    bool enableDirectRendering() const  { return mEnableDirectRendering; }
    bool strictBinding() const { return mStrictBinding; }

    void detect();

    QString driver() const  { return mDriver; }
    Version version() const  { return mVersion; }
    bool xgl() const { return mXgl; }


protected:

    void detectDriverAndVersion();
    void applyDriverSpecificOptions();
    static bool detectXgl();

    bool createGLXContext();
    void deleteGLXContext();


private:
    QString mGLVendor;
    QString mGLRenderer;
    QString mGLVersion;
    QString mDriver;
    Version mVersion;
    bool mXgl;

    bool mEnableCompositing;
    bool mEnableVSync;
    bool mEnableDirectRendering;
    bool mStrictBinding;

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    GLXContext mGLContext;
    Window mGLWindow;
#endif
};

}

#endif //KWIN_COMPOSITINGPREFS_H


