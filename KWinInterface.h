#ifndef KWIN_INTERFACE_H
#define KWIN_INTERFACE_H

#include <dcopobject.h>

class KWinInterface : virtual public DCOPObject
{
  K_DCOP

  k_dcop:

    virtual void updateClientArea() = 0;
};

#endif
