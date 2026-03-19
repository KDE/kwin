#pragma once
#include "item.h"

namespace KWin
{

class WindowItem;

/**
 * A helper item for background effects like blur. It represents the
 * effect in the scene and expands the repaint region when needed,
 * but it isn't (yet) involved in any rendering of its own.
 */
class KWIN_EXPORT BackgroundEffectItem : public Item
{
    Q_OBJECT

public:
    explicit BackgroundEffectItem(WindowItem *parentItem);

    void setEffectBoundingRect(const RectF &boundingRect);

    uint32_t pixelsToExpandRepaintsBelowOpaqueRegions() const;
    void setPixelsToExpandRepaintsBelowOpaqueRegions(uint32_t pixels);

private:
    void updateGeometry();

    uint32_t m_pixelsToExpandRepaintsBelowOpaqueRegions = 0;
    WindowItem *const m_windowItem;
    RectF m_effectBounds;
};

}
