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
#include <klibloader.h>

// X11/Qt conflict
#undef Unsorted

#include <qdir.h>
#include <qfile.h>

#include "plugins.h"
#include "kdedefault.h"

using namespace KWinInternal;

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
    insertItem(i18n("KDE 2"), 0);
    idCount = 1;
    idCurrent = 0;

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
    setItemChecked(idCurrent, true);
}

void PluginMenu::parseDesktop(QFileInfo *fi)
{
    QString tmpStr;
    KSimpleConfig config(fi->absFilePath(), true);
    config.setDesktopGroup();
    tmpStr = config.readEntry("X-KDE-Library", "");
    if(tmpStr.isEmpty()){
        qWarning("KWin: Invalid plugin: %s", fi->absFilePath().latin1());
        return;
    }
    fileList.append(tmpStr);
    if (tmpStr == mgr->currentPlugin())
       idCurrent = idCount;
    tmpStr = config.readEntry("Name", "");
    if(tmpStr.isEmpty())
        tmpStr = fi->baseName();
    insertItem(tmpStr, idCount);
    ++idCount;
}

void PluginMenu::slotActivated(int id)
{
    QString newPlugin;
    if (id > 0)
        newPlugin = fileList[id-1];

    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    config->writeEntry("PluginLib", newPlugin);
    config->sync();
    // We can't do this directly because we might destruct a client
    // underneath our own feet while doing so.
    QTimer::singleShot(0, mgr, SLOT(updatePlugin()));
}

PluginMgr::PluginMgr()
    : QObject()
{
    alloc_ptr = NULL;
    handle = 0;
    pluginStr = "standard";

    updatePlugin();
}

PluginMgr::~PluginMgr()
{
    if(handle)
        lt_dlclose(handle);
}

bool
PluginMgr::updatePlugin()
{
    KConfig *config = KGlobal::config();
    config->reparseConfiguration();
    config->setGroup("Style");
    QString newPlugin = config->readEntry("PluginLib", "standard" );
    if (newPlugin != pluginStr) {
       loadPlugin(newPlugin);
       return true;
    }
    return false;
}

Client* PluginMgr::allocateClient(Workspace *ws, WId w, bool tool)
{
    if(alloc_ptr)
        return(alloc_ptr(ws, w, tool));
    else
        return(new KDEClient(ws, w));
}

void PluginMgr::loadPlugin(QString nameStr)
{
    static bool dlregistered = false;
    lt_dlhandle oldHandle = handle;
    pluginStr = "standard";
    handle = 0;
    alloc_ptr = 0;

    if ( !nameStr.isEmpty() && nameStr != "standard" ) {
	if(!dlregistered){
	    dlregistered = true;
	    lt_dlinit();
	}
	QString path = KLibLoader::findLibrary(nameStr.latin1());

	if( !path.isEmpty() ) {
	    if ( (handle = lt_dlopen(path.latin1() ) ) ) {
		lt_ptr_t alloc_func = lt_dlsym(handle, "allocate");
		if(alloc_func) {
		    alloc_ptr = (Client* (*)(Workspace *ws, WId w, int tool))alloc_func;
		} else{
		    qWarning("KWin: %s is not a KWin plugin.", path.latin1());
		    lt_dlclose(handle);
		    handle = 0;
		}
	    } else {
		qWarning("KWin: cannot load client plugin %s.", path.latin1());
	    }
	}
    }
    if ( alloc_ptr )
	pluginStr = nameStr;

    emit resetAllClients();
    if(oldHandle)
        lt_dlclose(oldHandle);
}

#include "plugins.moc"

