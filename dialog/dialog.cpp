/****************************************************************************

 Copyright (C) 2003 Lubos Lunak        <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/

#include <kcmdlineargs.h>
#include <kapplication.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <klocale.h>
#include <unistd.h>
#include <kwin.h>
#include <X11/Xlib.h>

static const KCmdLineOptions options[] =
{ // no need for I18N_NOOP(), this is not supposed to be used directly
    { "message <message>", "Type of dialog message.", 0 },
    { "type <type>", "Dont show again type of dialog message.", 0 },
    { "window <wid>", "Window for which the dialog is shown.", 0 },
    { "+[data]", "Additional data.", 0 },
    KCmdLineLastOption
};

int main( int argc, char* argv[] )
{
    KCmdLineArgs::init( argc, argv, "kwin_dialog_helper", I18N_NOOP( "KWin" ),
	I18N_NOOP( "KWin helper utility" ), "1.0" );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
    KGlobal::locale()->insertCatalogue( "kwin" ); // the messages are in kwin's .po file
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    QString message = args->getOption( "message" );
    QString type = args->getOption( "type" );
    QString text;
    WId window = args->getOption( "window" ).toULong();
    if( message == "noborderaltf3" && args->count() == 1 )
        text = i18n( "You have selected to show a window without its border.\n"
                     "Without the border, you won't be able to enable the border "
                     "again using the mouse. Use the window operations menu instead, "
                     "activated using the %1 keyboard shortcut." ).arg( args->arg( 0 ));
    else if( message == "fullscreenaltf3" && args->count() == 1 )
        text = i18n( "You have selected to show a window in fullscreen mode.\n"
                     "If the application itself doesn't have an option to turn the fullscreen "
                     "mode off, you won't be able to disable it "
                     "again using the mouse. Use the window operations menu instead, "
                     "activated using the %1 keyboard shortcut." ).arg( args->arg( 0 ));
    else
        {
	KCmdLineArgs::usage( i18n( "This helper utility is not supposed to be called directly!" ));
	return 1;
        }
    if( window != 0 )
        KMessageBox::informationWId( window, text, "", type );
    else
        KMessageBox::information( NULL, text, "", type );
}
