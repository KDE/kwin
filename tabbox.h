/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef TABBOX_H
#define TABBOX_H
#include <qwidget.h>
#include <qtimer.h>

class QLabel;

namespace KWinInternal {

class Workspace;
class Client;
typedef QValueList<Client*> ClientList;

class TabBox : public QWidget
{
    Q_OBJECT
public:
    TabBox( Workspace *ws, const char *name=0 );
    ~TabBox();

    Client* currentClient();
    int currentDesktop();

    // DesktopMode and WindowsMode are based on the order in which the desktop
    //  or window were viewed.
    // DesktopListMode lists them in the order created.
    enum Mode { DesktopMode, DesktopListMode, WindowsMode };
    void setMode( Mode mode );
    Mode mode() const;

    void reset();
    void nextPrev( bool next = TRUE);

    void delayedShow();
    void hide();

    Workspace* workspace() const;

    void reconfigure();

protected:
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void hideEvent( QHideEvent* );
    void paintContents();

private:
    Client* client;
    Mode m;
    Workspace* wspace;
    ClientList clients;
    int desk;
    QLabel* icon;
    int wmax;
    QTimer delayedShowTimer;
    QString no_tasks;
    bool options_traverse_all;
};


/*!
  Returns the tab box' workspace
 */
inline Workspace* TabBox::workspace() const
{
    return wspace;
}

/*!
  Returns the current mode, either DesktopListMode or WindowsMode

  \sa setMode()
 */
inline TabBox::Mode TabBox::mode() const
{
    return m;
}

};

#endif
