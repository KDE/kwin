//////////////////////////////////////////////////////////////////////////////
// main.cpp
// oxygen-demo main
// -------------------
//
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "oxygenshadowdemodialog.h"

#include <KCmdLineArgs>
#include <KApplication>
#include <KIcon>
#include <KAboutData>
#include <config-workspace.h>

#include <cassert>

int main(int argc, char *argv[])
{
    KAboutData aboutData(
        "oxygen-shadow-demo",
        "kstyle_config",
        ki18n( "Oxygen Shadow Demo" ),
        WORKSPACE_VERSION_STRING,
        ki18n( "Oxygen decoration shadows demonstration" ),
        KAboutData::License_GPL_V2,
        ki18n( "(c) 2011, Hugo Pereira Da Costa" ));

    aboutData.addAuthor( ki18n( "Hugo Pereira Da Costa" ),KLocalizedString(), "hugo.pereira@free.fr" );

    KCmdLineArgs::init( argc, argv, &aboutData );
    KApplication app;

    app.setWindowIcon( KIcon( "oxygen" ) );

    // create dialog
    Oxygen::ShadowDemoDialog dialog;
    dialog.show();

    bool result = app.exec();
    return result;

}
