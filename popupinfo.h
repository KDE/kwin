/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef POPUPINFO_H
#define POPUPINFO_H
#include <qwidget.h>
#include <qtimer.h>
#include <qvaluelist.h>

class QLabel;

namespace KWinInternal {

class Workspace;
class Client;
typedef QValueList<Client*> ClientList;

class PopupInfo : public QWidget
{
    Q_OBJECT
public:
    PopupInfo( Workspace *ws, const char *name=0 );
    ~PopupInfo();

    Client* currentClient();
    int currentDesktop();

    // DesktopMode and WindowsMode are based on the order in which the desktop
    //  or window were viewed.
    // DesktopListMode lists them in the order created.
    enum Mode { DesktopMode, DesktopListMode, WindowsMode };
    void setMode( Mode mode );
    Mode mode() const;

    void reset();
    void hide();
    void showInfo();

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
    QTimer delayedHideTimer;
    QString no_tasks;
    bool options_traverse_all;
    int m_delayTime;
    bool m_show;
};


/*!
  Returns the tab box' workspace
 */
inline Workspace* PopupInfo::workspace() const
{
    return wspace;
}

/*!
  Returns the current mode, either DesktopListMode or WindowsMode

  \sa setMode()
 */
inline PopupInfo::Mode PopupInfo::mode() const
{
    return m;
}

};

#endif
