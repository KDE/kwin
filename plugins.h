/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#ifndef __PLUGINS_H
#define __PLUGINS_H

#include <qpopupmenu.h>
#include <qstringlist.h>
#include <ltdl.h>

class QFileInfo;

namespace KWinInternal {

class Client;
class Workspace;

class PluginMgr : public QObject
{
    Q_OBJECT
public:
    PluginMgr();
    ~PluginMgr();
    Client *allocateClient(Workspace *ws, WId w, bool tool);
    void loadPlugin(QString name);
    QString currentPlugin() { return pluginStr; }
public slots:
    bool updatePlugin();
    void resetPlugin();
signals:
    void resetAllClients();
protected:
    Client* (*alloc_ptr)(Workspace *ws, WId w, int tool);
    lt_dlhandle handle;
    QString pluginStr;
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
    int idCurrent;
    PluginMgr *mgr;
};

};

#endif
