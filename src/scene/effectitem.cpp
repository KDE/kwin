#include "effectitem.h"

namespace KWin
{

BackgroundEffectItem::BackgroundEffectItem(Item *parentItem)
    : Item(parentItem)
{
    // put it below the parent item
    setZ(-1);
}

}
