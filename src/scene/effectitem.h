#pragma once
#include "item.h"

namespace KWin
{

class GLTexture;

class KWIN_EXPORT BackgroundEffectItem : public Item
{
    Q_OBJECT
public:
    explicit BackgroundEffectItem(Item *parentItem);

    virtual void processBackground(GLTexture *backgroundTexture) = 0;
    virtual GLTexture *texture() const = 0;
};

}
