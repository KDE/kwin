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
    void updatePlugin();
    void resetPlugin();
signals:
    void resetAllClients();
protected:
    void shutdownKWin(const QString& error_msg);
    Client* (*alloc_ptr)(Workspace *ws, WId w, int tool);
    lt_dlhandle handle;
    QString pluginStr;
};

};

#endif
