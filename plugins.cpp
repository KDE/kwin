/*****************************************************************
kwin - the KDE window manager
								  
Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#include <kglobal.h>
#include <kconfig.h>
#include <kstddirs.h>
#include <kdesktopfile.h>
#include <ksimpleconfig.h>
#include <klocale.h>

// X11/Qt conflict
#undef Unsorted

#include <qdir.h>
#include <qfile.h>

#include "plugins.h"
#include "stdclient.h"

PluginMenu::PluginMenu(PluginMgr *manager, QWidget *parent, const char *name)
    : QPopupMenu(parent, name)
{
    connect(this, SIGNAL(aboutToShow()), SLOT(slotAboutToShow()));
    connect(this, SIGNAL(activated(int)), SLOT(slotActivated(int)));
    mgr = manager;
}

void PluginMenu::slotAboutToShow()
{
    clear();
    fileList.clear();
    insertItem(i18n("Standard"), 0);
    idCount = 1;

    QDir dir;
    dir.setFilter(QDir::Files);
    const QFileInfoList *list;
    int count = KGlobal::dirs()->findDirs("data", "kwin").count();
    if(count){
        dir.setPath(KGlobal::dirs()->findDirs("data", "kwin")
                    [count > 1 ? 1 : 0]);
        if(dir.exists()){
            list =  dir.entryInfoList();
            if(list){
                QFileInfoListIterator it(*list);
                QFileInfo *fi;
                for(; (fi = it.current()) != NULL; ++it){
                    if(KDesktopFile::isDesktopFile(fi->absFilePath()))
                        parseDesktop(fi);
                }
            }
        }
        if(count > 1){
            dir.setPath(KGlobal::dirs()->findDirs("data", "kwin")[0]);
            if(dir.exists()){
                list = dir.entryInfoList();
                if(list){
                    QFileInfoListIterator it(*list);
                    QFileInfo *fi;
                    for(; (fi = it.current()) != NULL; ++it){
                        if(KDesktopFile::isDesktopFile(fi->absFilePath()))
                            parseDesktop(fi);
                    }
                }
            }
        }
    }
}

void PluginMenu::parseDesktop(QFileInfo *fi)
{
    QString tmpStr;
    KSimpleConfig config(fi->absFilePath(), true);
    config.setDesktopGroup();
    tmpStr = config.readEntry("X-KDE-Library", "");
    if(tmpStr.isEmpty()){
        warning("KWin: Invalid plugin: %s", fi->absFilePath().latin1());
        return;
    }
    fileList.append(tmpStr);
    tmpStr = config.readEntry("Name", "");
    if(tmpStr.isEmpty())
        tmpStr = fi->baseName();
    insertItem(tmpStr, idCount);
    ++idCount;
}

void PluginMenu::slotActivated(int id)
{
    if(id == 0)
        mgr->loadPlugin(QString::null);
    else
        mgr->loadPlugin(fileList[id-1]);
}

PluginMgr::PluginMgr()
    : QObject()
{
    alloc_ptr = NULL;
    handle = 0;

    QString pluginStr;
    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    pluginStr = config->readEntry("PluginLib", "standard");
    if(pluginStr.isEmpty() || pluginStr == "standard")
        return;
    else
        loadPlugin(pluginStr);
}

PluginMgr::~PluginMgr()
{
    if(handle)
        lt_dlclose(handle);
}

Client* PluginMgr::allocateClient(Workspace *ws, WId w)
{
    if(alloc_ptr)
        return(alloc_ptr(ws, w));
    else
        return(new StdClient(ws, w));
}

void PluginMgr::loadPlugin(QString nameStr)
{
    static bool dlregistered = false;
    static lt_dlhandle oldHandle = 0;

    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    config->writeEntry("PluginLib", nameStr);
    oldHandle = handle;

    if(nameStr.isNull()){
        handle = 0;
        alloc_ptr = NULL;
        config->writeEntry("PluginLib", "standard");
    }
    else{
        if(!dlregistered){
            dlregistered = true;
            lt_dlinit();
        }
        nameStr += ".la";
        nameStr = KGlobal::dirs()->findResource("lib", nameStr);

        if(!nameStr){
            warning("KWin: cannot find client plugin.");
            handle = 0;
            alloc_ptr = NULL;
            config->writeEntry("PluginLib", "standard");
        }
        else{
            handle = lt_dlopen(nameStr.latin1());
            if(!handle){
                warning("KWin: cannot load client plugin %s.", nameStr.latin1());
                handle = 0;
                alloc_ptr = NULL;
                config->writeEntry("PluginLib", "standard");
            }
            else{
                lt_ptr_t alloc_func = lt_dlsym(handle, "allocate");
                if(alloc_func)
                    alloc_ptr = (Client* (*)(Workspace *ws, WId w))alloc_func;
                else{
                    warning("KWin: %s is not a KWin plugin.", nameStr.latin1());
                    lt_dlclose(handle);
                    handle = 0;
                    alloc_ptr = NULL;
                    config->writeEntry("PluginLib", "standard");
                }
            }
        }
    }
    config->sync();
    emit resetAllClients();
    if(oldHandle)
        lt_dlclose(oldHandle);
}

#include "plugins.moc"

