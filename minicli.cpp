// minicli
// Copyright (C) 1997 Matthias Ettrich <ettrich@kde.org>
// Copyright (c) 1999 Preston Brown <pbrown@kde.org>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <qdir.h>
#include <qwindowdefs.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qlist.h>
#include <qkeycode.h>
#include <qlayout.h>

#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kbuttonbox.h>

#include <X11/Xlib.h>
#include <kprocess.h>

#include "client.h"
#include "main.h"


#include "minicli.moc"

#ifdef KeyPress
#undef KeyPress
#endif

//KURT: not sure what this variable does
//extern bool do_not_draw;

// global history list
QList <char> *history = 0;
QListIterator <char> *it;

/*
   Function determines whether what the user entered in the minicli
   box is executable or not. This code is eniterly based on the C
   version of the 'which' command in csh/bash. (Dawit A.)
*/
bool isExecutable ( const char *name )
{
  QString test;
  char *path = getenv( "PATH" );
  char *pc = path;
  bool found = false;

  while ( *pc != '\0' && found == false ) {
    int len = 0;
    while ( *pc != ':' && *pc != '\0' ) {
      len++;
      pc++;
    }
    char save = *pc;
    *pc = '\0';
    test = QString(pc-len) + "/" + name;
    *pc = save;
    if (*pc) { pc++; }
    found = ( access(test.data(), 01) == 0 );  /* is it executable ? */
  }
  return found;
}

// Check for the existance of a local file/directory. (Dawit A.)
bool isLocalResource ( const char * cmd )
{
  struct stat buff;
  return ( stat( cmd, &buff ) == 0 && ( S_ISREG( buff.st_mode ) || S_ISDIR( buff.st_mode ) ) ) ? true :false;
}

bool isValidShortURL ( const char * cmd )
{
  // NOTE : By design, this check disqualifies some valid
  // URL's that contain queries and *nix command characters.
  // This is an intentional trade off to best much the URL
  // with a local resource first.  This also allows minicli
  // to behave consistently with the way it does now. (Dawit A.)
  char lastchr = *( cmd + strlen (cmd) - 1 );
  //debug ( "%s : %c", "The last charater of the URL is", lastchr );
  if ( strchr ( cmd, ' ' ) != 0L || strchr ( cmd, ';' ) != 0L )
    return false;
  if ( *cmd  == '/'  ||  lastchr == '&' )
    return  false;
  if ( strchr ( cmd, '<' ) != 0L ||  strchr ( cmd, '>' ) != 0L )
    return false;
  if ( strstr ( cmd, "||" ) != 0L || strstr ( cmd, "&&" ) != 0L )
    return false;
  return true;
}

void execute( const char* cmd, bool inTerminal)
{
  QString tmp;

  // Torben
  // WWW Adress ?
  if ( strnicmp( cmd, "www.", 4 ) == 0 ) {
    tmp = "kfmclient exec http://";
    tmp += cmd;
    cmd = tmp.data();
  }
  // FTP Adress ?
  else if ( strnicmp( cmd, "ftp.", 4 ) == 0 ) {
    tmp = "kfmclient exec ftp://";
    tmp += cmd;
    cmd = tmp.data();
  }
  // Looks like a KDEHelp thing ?
  else if ( strstr( cmd, "man:" ) != 0L ||
	    strstr( cmd, "MAN:" ) != 0L || cmd[0] == '#' ) {
    tmp = "kdehelp \"";
    if ( cmd[0] == '#' ) {
      tmp += "man:";
      tmp += cmd + 1;
    } else
      tmp += cmd;
    tmp += "\"";
    cmd = tmp.data();
  }
  // Looks like an URL ?
  else if ( strstr( cmd, "://" ) != 0L ||
            strnicmp ( cmd, "news:", 5) == 0 ||
            strnicmp ( cmd, "mailto:", 7) == 0 ) {
    tmp = "kfmclient exec ";
    tmp += cmd;
    cmd = tmp.data();
  }
  // Usual file or directory
  else {
    const char *p = cmd;
    QString tst;

    if ( strnicmp( p, "file:", 5 ) == 0 )
      p = p + 5;
    // Replace '~' with the user's home directory.
    if ( *p == '~' )
      {
        p = tst.append( p ).replace( 0, 1, QDir::homeDirPath() ).data();
        cmd = p;
      }
    bool isLocal = isLocalResource ( cmd );   // Am I locally available ?
    bool isExec = isExecutable ( cmd );       // Am I locally executable ?

    // Is this a non-executable local resource ?
    if ( !isExec && isLocal ) {
      // Tell KFM to open the document
      tmp += cmd;
      // Quote the URL in case there is a space in it ; so
      // one can for example type : ~/Desktop/CD ROM.kdelnk
      // to mount the CD ROM device. ( Dawit A. )
      tmp.prepend ( "kfmclient exec \"" );
      tmp.append ( "\"");
      cmd = tmp.data();
    }
    // if URL is not a local resource and does not contain any *nix shell
    // command characters, append "http://" as the default protocol. (Dawit A.)
    else if ( !isExec && isValidShortURL ( cmd ) ) {
      tmp = cmd;
      tmp.prepend ("kfmclient exec http://");
      cmd = tmp.data();
    } else if (isExec && inTerminal) {
      tmp += cmd;
      tmp.prepend("konsole -e \"");
      tmp.append("\"");
      cmd = tmp.data();
      qDebug("cmd is now %s",cmd);
    }
  }

  KShellProcess proc;
  proc << cmd;
  proc.start(KShellProcess::DontCare);
}

Minicli::Minicli( Workspace *ws, QWidget *parent, const char *name, WFlags f)
  : QVBox(parent, name, f & WStyle_StaysOnTop)
{
  setFrameStyle(QFrame::WinPanel | QFrame:: Raised);

  setSpacing(7);
  setMargin(7);

  QHBox *hBox = new QHBox(this);
  hBox->setSpacing(7);

  QLabel *label = new QLabel(hBox);
  label->setPixmap(KGlobal::iconLoader()->loadApplicationIcon("go"));

  label = new QLabel(i18n("Type in the name of the program or command you wish to execute.\n"
			  "Check the \"run in terminal\" box if the program is not graphical."), hBox);

  hBox = new QHBox(this);
  hBox->setSpacing(7);

  label = new QLabel(i18n("Command:"), hBox);
  label->setFixedSize(label->sizeHint());
  runCombo = new QComboBox(true, hBox);
  runCombo->setAutoCompletion(true);

  // populate run combo with history.
  KConfig *config = KGlobal::config();
  config->setGroup("MiniCli");
  QStrList hist;
  config->readListEntry("History", hist);

  //CT 15Jan1999 - limit in-memory history too
  unsigned int max_hist = config->readNumEntry("HistoryLength", 50);
  for (unsigned int i=0; i< QMIN(hist.count(), max_hist); i++)
    if (strcmp(hist.at(i),"") != 0)
      runCombo->insertItem(hist.at(i));

  // insert blank entry at end and make it active
  runCombo->insertItem("");

  hBox = new QHBox(this);
  hBox->setSpacing(7);
  terminalBox = new QCheckBox(i18n("Run in terminal"), hBox);

  KButtonBox *bbox = new KButtonBox(hBox);
  bbox->addStretch();
  QPushButton *runButton = bbox->addButton(i18n("&Run"), false);
  runButton->setDefault(true);
  connect(runButton, SIGNAL(clicked()),
	  this, SLOT(run_command()));

  QPushButton *cancelButton = bbox->addButton(i18n("&Cancel"), false);
  connect(cancelButton, SIGNAL(clicked()),
	  this, SLOT(cleanup()));

  adjustSize();

  workspace = ws;

  move(QApplication::desktop()->width()/2 - width()/2,
       QApplication::desktop()->height()/2 - height()/2);
}

void Minicli::keyPressEvent(QKeyEvent *kev)
{
  int a = ((QKeyEvent*)kev)->ascii();
  if (a == 3 || a == 7 || a == 27) {
    cleanup();
    kev->accept();
    return;
  }
  if (kev->key() == Key_Return ||
      kev->key() == Key_Enter) {
    kev->accept();
    run_command();
    return;
  }

  kev->ignore();
}

bool Minicli::do_grabbing()
{
  runCombo->setCurrentItem(runCombo->count() - 1);
  terminalBox->setChecked(false);
  show();

  reactive = workspace->activeClient();
  if (reactive)
    reactive->setActive(false);
  XSetInputFocus (qt_xdisplay(), winId(), RevertToParent, CurrentTime);

  // commenting out the following makes "dead keys" (@, ~, ...) work
  //  if (XGrabKeyboard(qt_xdisplay(), runCombo->winId(),True,GrabModeAsync,
  //		    GrabModeAsync,CurrentTime) != GrabSuccess)
  //    return False;

// KURT: not sure what this does
//  do_not_draw = true;
  raise();
  runCombo->setFocus();
  return True;
}

void Minicli::run_command(){
  QString s = runCombo->currentText();
  s = s.stripWhiteSpace();

  if (s.simplifyWhiteSpace().stripWhiteSpace().isEmpty()) {
    cleanup();
    return;
  }

  KGlobal::config()->setGroup("MiniCli");
  int max_hist = KGlobal::config()->readNumEntry("HistoryLength",50);

  runCombo->removeItem(runCombo->count() - 1);

  if (runCombo->text(runCombo->count() - 1).simplifyWhiteSpace().isEmpty()) {
    runCombo->removeItem(runCombo->count() - 1); // remove empty item
  }
  if (s != runCombo->text(runCombo->count() - 1)) { // don't save duplicates
    runCombo->insertItem(s);
  }

  runCombo->insertItem("");
  while (runCombo->count() > max_hist)
    runCombo->removeItem(0); // take out first item

  bool term = terminalBox->isChecked();
  cleanup();

//  if (s == "logout")
//    logout();
//  else
    execute(s.data(), term);
}

void Minicli::cleanup()
{
  hide();
  terminalBox->setChecked(false);
  runCombo->setCurrentItem(runCombo->count() - 1);

// KURT: not sure what this does
//  do_not_draw = false;
  if (reactive && workspace->hasClient(reactive)){
      reactive->setActive(true);
      XSetInputFocus (qt_xdisplay(), reactive->window(),
		      RevertToPointerRoot, CurrentTime);
  }
  XSync(qt_xdisplay(), false);

  // save history list (M.H.)
  KConfig *config = KGlobal::config();
  config->setGroup("MiniCli");

  QStrList hist;
  for (int i = 0; i < runCombo->count(); i++) {
    if (runCombo->text(i).simplifyWhiteSpace() != "")
      hist.append(runCombo->text(i).utf8());
  }

  config->writeEntry("History", hist);
  config->sync();
}

void Minicli::commandCompletion(){
  QListIterator <char> *it;
  it = new QListIterator <char> (*history);
  QString s,t,u;
  s = runCombo->text(runCombo->currentItem());
  bool abort = True;

  if (!it->isEmpty()){
    it->toLast();
    while (!it->atFirst()){
      --(*it);
      t = it->current();
      if (t.length()>=s.length() && t.left(s.length()) == s){
	if (t.length()>s.length()){
	  u = t.left(s.length()+1);
	  abort = False;
	}
	it->toFirst();
      }
    }
    if (!abort){
      it->toLast();
      while (!it->atFirst()){
	--(*it);
	t = it->current();
	if (t.length()>=s.length() && t.left(s.length()) == s){
	  if (t.length()<u.length()
	      || t.left(u.length()) != u){
	    abort = True;
	    it->toFirst();
	  }
	}
      }
    }

    if (!abort){
      runCombo->insertItem(u.data());
      commandCompletion();
    }
  }
  delete it;
  return;
}
