#ifndef TABBOX_H
#define TABBOX_H
#include <qwidget.h>

class Workspace;
class Client;
class QLabel;
typedef QValueList<Client*> ClientList;

class TabBox : public QWidget
{
    Q_OBJECT
public:
    TabBox( Workspace *ws, const char *name=0 );
    ~TabBox();

    Client* currentClient();
    int currentDesktop();

    enum Mode { DesktopMode, WindowsMode };
    void setMode( Mode mode );
    Mode mode() const;

    void reset();
    void nextPrev( bool next = TRUE);

    Workspace* workspace() const;

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

};


/*!
  Returns the tab box' workspace
 */
inline Workspace* TabBox::workspace() const
{
    return wspace;
}

/*!
  Returns the current mode, either DesktopMode or WindowsMode

  \sa setMode()
 */
inline TabBox::Mode TabBox::mode() const
{
    return m;
}

#endif
