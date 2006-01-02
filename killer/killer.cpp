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
#include <QX11Info>

static const KCmdLineOptions options[] =
    {
    // no need for I18N_NOOP(), this is not supposed to be used directly
        { "pid <pid>", "PID of the application to terminate.", 0 },
        { "hostname <hostname>", "Hostname on which the application is running.", 0 },
        { "windowname <caption>", "Caption of the window to be terminated.", 0 },
        { "applicationname <name>", "Name of the application to be terminated.", 0 },
        { "wid <id>", "ID of resource belonging to the application.", 0 },
        { "timestamp <time>", "Time of user action causing killing.", 0 },
        KCmdLineLastOption
    };

int main( int argc, char* argv[] )
    {
    KLocale::setMainCatalog( "kwin" ); // the messages are in kwin's .po file
    KCmdLineArgs::init( argc, argv, "kwin_killer_helper", I18N_NOOP( "KWin" ),
	I18N_NOOP( "KWin helper utility" ), "1.0" );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    QByteArray hostname = args->getOption( "hostname" );
    bool pid_ok = false;
    pid_t pid = QString( args->getOption( "pid" ) ).toULong( &pid_ok );
    QString caption = QString::fromUtf8( args->getOption( "windowname" ));
    QString appname = QString::fromLatin1( args->getOption( "applicationname" ));
    bool id_ok = false;
    Window id = QString( args->getOption( "wid" ) ).toULong( &id_ok );
    bool time_ok = false;
    Time timestamp =QString( args->getOption( "timestamp" ) ).toULong( &time_ok );
    args->clear();
    if( !pid_ok || pid == 0 || !id_ok || id == None || !time_ok || timestamp == CurrentTime
	|| hostname.isEmpty() || caption.isEmpty() || appname.isEmpty())
        {
	KCmdLineArgs::usage( i18n( "This helper utility is not supposed to be called directly." ));
	return 1;
        }
    QString question = i18n(
	"<qt>Window with title \"<b>%2</b>\" is not responding. "
	"This window belongs to application <b>%1</b> (PID=%3, hostname=%4).<p>"
	"Do you wish to terminate this application? (All unsaved data in this application will be lost.)</qt>" )
	.arg( appname ).arg( caption ).arg( pid ).arg( QString( hostname ) );
    app.updateUserTimestamp( timestamp );
    if( KMessageBox::warningYesNoWId( id, question, QString(), i18n("Terminate"), i18n("Keep Running") ) == KMessageBox::Yes )
        {    
	if( hostname != "localhost" )
            {
    	    KProcess proc;
	    proc << "xon" << hostname << "kill" << QString::number( pid );
    	    proc.start( KProcess::DontCare );
	    }
	else
	    ::kill( pid, SIGKILL );
	XKillClient( QX11Info::display(), id );
        }
    }
