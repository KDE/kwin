// minicli
// Copyright (C) 1997 Matthias Ettrich <ettrich@kde.org>
// Copyright (c) 1999 Preston Brown <pbrown@kde.org>
//
// Torben added command completion
// 09.11.97
//
// Dawit added short URL support
// 02.26.99

#ifndef MINICLI_H
#define MINICLI_H

#include <kurlcompletion.h>
#include <qvbox.h>

void execute ( const char*, bool );
bool isExecutable ( const char* );
bool isLocalResource ( const char* );
bool isValidShortURL ( const char * );

class QComboBox;
class QLabel;
class QCheckBox;

class Workspace;

class Minicli : public QVBox {
  Q_OBJECT
public:
  Minicli( Workspace *ws = 0, QWidget *parent=0, const char *name=0, WFlags f=0);
  bool do_grabbing();

public slots:
  void cleanup();

protected:
  void keyPressEvent(QKeyEvent *);

private slots:
  void run_command();

private:
  QComboBox* runCombo;
  QCheckBox *terminalBox;
  Client* reactive;
  void commandCompletion();

  Workspace *workspace;

  KURLCompletion kurlcompletion;
};

#endif

