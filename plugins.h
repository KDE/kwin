/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#ifndef __PLUGINS_H
#define __PLUGINS_H

#include <qpopupmenu.h>
#include <qstringlist.h>
#include <ltdl.h>


class Client;
class Workspace;

class QFileInfo;

class PluginMgr : public QObject
{
    Q_OBJECT
public:
    PluginMgr();
    ~PluginMgr();
    Client *allocateClient(Workspace *ws, WId w);
    void loadPlugin(QString name);
signals:
    void resetAllClients();
protected:
    Client* (*alloc_ptr)(Workspace *ws, WId w);
    lt_dlhandle handle;
};

class PluginMenu : public QPopupMenu
{
    Q_OBJECT
public:
    PluginMenu(PluginMgr *manager, QWidget *parent=0, const char *name=0);
protected slots:
    void slotAboutToShow();
    void slotActivated(int id);
protected:
    void parseDesktop(QFileInfo *fi);
    QStringList fileList;
    int idCount;
    PluginMgr *mgr;
};


#endif
