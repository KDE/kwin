/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
    bool enableCompositing() const  { return mEnableCompositing; }
    bool enableVSync() const  { return mEnableVSync; }
    bool enableDirectRendering() const  { return mEnableDirectRendering; }

    void detect();

    QString driver() const  { return mDriver; }
    Version version() const  { return mVersion; }


protected:

    void detectDriverAndVersion();
    void applyDriverSpecificOptions();

    bool createGLXContext();
    void deleteGLXContext();


private:
    QString mDriver;
    Version mVersion;

    bool mEnableCompositing;
    bool mEnableVSync;
    bool mEnableDirectRendering;

#ifdef HAVE_OPENGL
    GLXContext mGLContext;
    Window mGLWindow;
#endif
};

}

#endif //KWIN_COMPOSITINGPREFS_H


