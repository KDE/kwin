/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef POPUPINFO_H
#define POPUPINFO_H
#include <qwidget.h>
#include <qtimer.h>
#include <qvaluelist.h>

namespace KWinInternal {

class Workspace;

class PopupInfo : public QWidget
{
    Q_OBJECT
public:
    PopupInfo( const char *name=0 );
    ~PopupInfo();

    void reset();
    void hide();
    void showInfo(QString infoString);

    void reconfigure();

protected:
    void paintEvent( QPaintEvent* );
    void paintContents();

private:
    QTimer m_delayedHideTimer;
    int m_delayTime;
    bool m_show;
    bool m_shown;
    QString m_infoString;
};

};

#endif
