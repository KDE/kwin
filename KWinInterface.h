#ifndef KWIN_INTERFACE_H
#define KWIN_INTERFACE_H

#include <dcopobject.h>

class KWinInterface : virtual public DCOPObject
    {
    K_DCOP

    k_dcop:

    virtual ASYNC cascadeDesktop() = 0;
    virtual ASYNC unclutterDesktop() = 0;
    virtual ASYNC reconfigure() = 0;
    virtual ASYNC killWindow() = 0;
    virtual void refresh() = 0;
    virtual void doNotManage(QString)= 0;
    virtual void showWindowMenuAt(unsigned long winId, int x, int y)= 0;
    virtual void setDesktopLayout(int orientation, int x, int y)= 0;
    virtual bool setCurrentDesktop(int)= 0;
    virtual int currentDesktop() const = 0;
    virtual void nextDesktop() = 0;
    virtual void previousDesktop() = 0;
    virtual void circulateDesktopApplications() = 0;

    k_dcop_signals:

    // never emitted  
    virtual void dcopResetAllClients();
    };

#endif
