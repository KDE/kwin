/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#ifndef KWIN_PLUGINS_H
#define KWIN_PLUGINS_H

#include <qpopupmenu.h>
#include <qstringlist.h>
#include <netwm_def.h>

class QFileInfo;
class KLibrary;

namespace KWinInternal {

class Client;
class Workspace;

class PluginMgr : public QObject
{
    Q_OBJECT
public:
    PluginMgr();
    ~PluginMgr();
    Client *createClient(Workspace *ws, WId w, NET::WindowType type);
    bool loadPlugin(QString name);
    QString currentPlugin() { return pluginStr; }
public slots:
    void updatePlugin();
signals:
    void resetAllClients();
protected:
    void shutdownKWin(const QString& error_msg);
    Client* (*create_ptr)(Workspace *ws, WId w, NET::WindowType type);
    Client* (*old_create_ptr)(Workspace *ws, WId w, int tool);
    KLibrary *library;
    QString pluginStr;
    QCString defaultPlugin;
};

};

#endif
