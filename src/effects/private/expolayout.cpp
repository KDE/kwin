/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    // The layouting code is taken from the present windows effect.
    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "expolayout.h"

#include <cmath>

ExpoCell::ExpoCell(QObject *parent)
    : QObject(parent)
{
}

ExpoCell::~ExpoCell()
{
    setLayout(nullptr);
}

ExpoLayout *ExpoCell::layout() const
{
    return m_layout;
}

void ExpoCell::setLayout(ExpoLayout *layout)
{
    if (m_layout == layout) {
        return;
    }
    if (m_layout) {
        m_layout->removeCell(this);
    }
    m_layout = layout;
    if (m_layout && m_enabled) {
        m_layout->addCell(this);
    }
    Q_EMIT layoutChanged();
}

bool ExpoCell::isEnabled() const
{
    return m_enabled;
}

void ExpoCell::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (enabled) {
            if (m_layout) {
                m_layout->addCell(this);
            }
        } else {
            if (m_layout) {
                m_layout->removeCell(this);
            }
        }
        Q_EMIT enabledChanged();
    }
}

void ExpoCell::update()
{
    if (m_layout) {
        m_layout->polish();
    }
}

int ExpoCell::naturalX() const
{
    return m_naturalX;
}

void ExpoCell::setNaturalX(int x)
{
    if (m_naturalX != x) {
        m_naturalX = x;
        update();
        Q_EMIT naturalXChanged();
    }
}

int ExpoCell::naturalY() const
{
    return m_naturalY;
}

void ExpoCell::setNaturalY(int y)
{
    if (m_naturalY != y) {
        m_naturalY = y;
        update();
        Q_EMIT naturalYChanged();
    }
}

int ExpoCell::naturalWidth() const
{
    return m_naturalWidth;
}

void ExpoCell::setNaturalWidth(int width)
{
    if (m_naturalWidth != width) {
        m_naturalWidth = width;
        update();
        Q_EMIT naturalWidthChanged();
    }
}

int ExpoCell::naturalHeight() const
{
    return m_naturalHeight;
}

void ExpoCell::setNaturalHeight(int height)
{
    if (m_naturalHeight != height) {
        m_naturalHeight = height;
        update();
        Q_EMIT naturalHeightChanged();
    }
}

QRect ExpoCell::naturalRect() const
{
    return QRect(naturalX(), naturalY(), naturalWidth(), naturalHeight());
}

QMargins ExpoCell::margins() const
{
    return m_margins;
}

int ExpoCell::x() const
{
    return m_x.value_or(0);
}

void ExpoCell::setX(int x)
{
    if (m_x != x) {
        m_x = x;
        Q_EMIT xChanged();
    }
}

int ExpoCell::y() const
{
    return m_y.value_or(0);
}

void ExpoCell::setY(int y)
{
    if (m_y != y) {
        m_y = y;
        Q_EMIT yChanged();
    }
}

int ExpoCell::width() const
{
    return m_width.value_or(0);
}

void ExpoCell::setWidth(int width)
{
    if (m_width != width) {
        m_width = width;
        Q_EMIT widthChanged();
    }
}

int ExpoCell::height() const
{
    return m_height.value_or(0);
}

void ExpoCell::setHeight(int height)
{
    if (m_height != height) {
        m_height = height;
        Q_EMIT heightChanged();
    }
}

QString ExpoCell::persistentKey() const
{
    return m_persistentKey;
}

void ExpoCell::setPersistentKey(const QString &key)
{
    if (m_persistentKey != key) {
        m_persistentKey = key;
        update();
        Q_EMIT persistentKeyChanged();
    }
}

int ExpoCell::bottomMargin() const
{
    return m_margins.bottom();
}

void ExpoCell::setBottomMargin(int margin)
{
    if (m_margins.bottom() != margin) {
        m_margins.setBottom(margin);
        update();
        Q_EMIT bottomMarginChanged();
    }
}

ExpoLayout::ExpoLayout(QQuickItem *parent)
    : QQuickItem(parent)
{
}

ExpoLayout::LayoutMode ExpoLayout::mode() const
{
    return m_mode;
}

void ExpoLayout::setMode(LayoutMode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        polish();
        Q_EMIT modeChanged();
    }
}

bool ExpoLayout::fillGaps() const
{
    return m_fillGaps;
}

void ExpoLayout::setFillGaps(bool fill)
{
    if (m_fillGaps != fill) {
        m_fillGaps = fill;
        polish();
        Q_EMIT fillGapsChanged();
    }
}

int ExpoLayout::spacing() const
{
    return m_spacing;
}

void ExpoLayout::setSpacing(int spacing)
{
    if (m_spacing != spacing) {
        m_spacing = spacing;
        polish();
        Q_EMIT spacingChanged();
    }
}

bool ExpoLayout::isReady() const
{
    return m_ready;
}

void ExpoLayout::setReady()
{
    if (!m_ready) {
        m_ready = true;
        Q_EMIT readyChanged();
    }
}

void ExpoLayout::forceLayout()
{
    updatePolish();
}

void ExpoLayout::updatePolish()
{
    if (!m_cells.isEmpty()) {
        switch (m_mode) {
        case LayoutClosest:
            calculateWindowTransformationsClosest();
            break;
        case LayoutNatural:
            calculateWindowTransformationsNatural();
            break;
        case LayoutNone:
            resetTransformations();
            break;
        }
    }

    setReady();
}

void ExpoLayout::addCell(ExpoCell *cell)
{
    Q_ASSERT(!m_cells.contains(cell));
    m_cells.append(cell);
    polish();
}

void ExpoLayout::removeCell(ExpoCell *cell)
{
    m_cells.removeOne(cell);
    polish();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void ExpoLayout::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
#else
void ExpoLayout::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
#endif
{
    if (newGeometry.size() != oldGeometry.size()) {
        polish();
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
#else
    QQuickItem::geometryChange(newGeometry, oldGeometry);
#endif
}

static int distance(const QPoint &a, const QPoint &b)
{
    const int xdiff = a.x() - b.x();
    const int ydiff = a.y() - b.y();
    return int(std::sqrt(qreal(xdiff * xdiff + ydiff * ydiff)));
}

static QRect centered(ExpoCell *cell, const QRect &bounds)
{
    const QSize scaled = QSize(cell->naturalWidth(), cell->naturalHeight())
                             .scaled(bounds.size(), Qt::KeepAspectRatio);

    return QRect(bounds.center().x() - scaled.width() / 2,
                 bounds.center().y() - scaled.height() / 2,
                 scaled.width(),
                 scaled.height());
}

void ExpoLayout::calculateWindowTransformationsClosest()
{
    QRect area = QRect(0, 0, width(), height());
    const int columns = int(std::ceil(std::sqrt(qreal(m_cells.count()))));
    const int rows = int(std::ceil(m_cells.count() / qreal(columns)));

    // Assign slots
    const int slotWidth = area.width() / columns;
    const int slotHeight = area.height() / rows;
    QVector<ExpoCell *> takenSlots;
    takenSlots.resize(rows * columns);
    takenSlots.fill(nullptr);

    // precalculate all slot centers
    QVector<QPoint> slotCenters;
    slotCenters.resize(rows * columns);
    for (int x = 0; x < columns; ++x) {
        for (int y = 0; y < rows; ++y) {
            slotCenters[x + y * columns] = QPoint(area.x() + slotWidth * x + slotWidth / 2,
                                                  area.y() + slotHeight * y + slotHeight / 2);
        }
    }

    // Assign each window to the closest available slot
    QList<ExpoCell *> tmpList = m_cells; // use a QLinkedList copy instead?
    while (!tmpList.isEmpty()) {
        ExpoCell *cell = tmpList.first();
        int slotCandidate = -1, slotCandidateDistance = INT_MAX;
        const QPoint pos = cell->naturalRect().center();

        for (int i = 0; i < columns * rows; ++i) { // all slots
            const int dist = distance(pos, slotCenters[i]);
            if (dist < slotCandidateDistance) { // window is interested in this slot
                ExpoCell *occupier = takenSlots[i];
                Q_ASSERT(occupier != cell);
                if (!occupier || dist < distance(occupier->naturalRect().center(), slotCenters[i])) {
                    // either nobody lives here, or we're better - takeover the slot if it's our best
                    slotCandidate = i;
                    slotCandidateDistance = dist;
                }
            }
        }
        Q_ASSERT(slotCandidate != -1);
        if (takenSlots[slotCandidate]) {
            tmpList << takenSlots[slotCandidate]; // occupier needs a new home now :p
        }
        tmpList.removeAll(cell);
        takenSlots[slotCandidate] = cell; // ...and we rumble in =)
    }

    for (int slot = 0; slot < columns * rows; ++slot) {
        ExpoCell *cell = takenSlots[slot];
        if (!cell) { // some slots might be empty
            continue;
        }

        // Work out where the slot is
        QRect target(area.x() + (slot % columns) * slotWidth,
                     area.y() + (slot / columns) * slotHeight,
                     slotWidth, slotHeight);
        target.adjust(m_spacing, m_spacing, -m_spacing, -m_spacing); // Borders
        target = target.marginsRemoved(cell->margins());

        qreal scale;
        if (target.width() / qreal(cell->naturalWidth()) < target.height() / qreal(cell->naturalHeight())) {
            // Center vertically
            scale = target.width() / qreal(cell->naturalWidth());
            target.moveTop(target.top() + (target.height() - int(cell->naturalHeight() * scale)) / 2);
            target.setHeight(int(cell->naturalHeight() * scale));
        } else {
            // Center horizontally
            scale = target.height() / qreal(cell->naturalHeight());
            target.moveLeft(target.left() + (target.width() - int(cell->naturalWidth() * scale)) / 2);
            target.setWidth(int(cell->naturalWidth() * scale));
        }
        // Don't scale the windows too much
        if (scale > 2.0 || (scale > 1.0 && (cell->naturalWidth() > 300 || cell->naturalHeight() > 300))) {
            scale = (cell->naturalWidth() > 300 || cell->naturalHeight() > 300) ? 1.0 : 2.0;
            target = QRect(
                target.center().x() - int(cell->naturalWidth() * scale) / 2,
                target.center().y() - int(cell->naturalHeight() * scale) / 2,
                scale * cell->naturalWidth(), scale * cell->naturalHeight());
        }

        cell->setX(target.x());
        cell->setY(target.y());
        cell->setWidth(target.width());
        cell->setHeight(target.height());
    }
}

static inline int heightForWidth(ExpoCell *cell, int width)
{
    return int((width / qreal(cell->naturalWidth())) * cell->naturalHeight());
}

static bool isOverlappingAny(ExpoCell *w, const QHash<ExpoCell *, QRect> &targets, const QRegion &border, int spacing)
{
    QHash<ExpoCell *, QRect>::const_iterator winTarget = targets.find(w);
    if (winTarget == targets.constEnd()) {
        return false;
    }
    if (border.intersects(*winTarget)) {
        return true;
    }
    const QMargins halfSpacing(spacing / 2, spacing / 2, spacing / 2, spacing / 2);

    // Is there a better way to do this?
    QHash<ExpoCell *, QRect>::const_iterator target;
    for (target = targets.constBegin(); target != targets.constEnd(); ++target) {
        if (target == winTarget) {
            continue;
        }
        if (winTarget->marginsAdded(halfSpacing).intersects(target->marginsAdded(halfSpacing))) {
            return true;
        }
    }
    return false;
}

void ExpoLayout::calculateWindowTransformationsNatural()
{
    const QRect area = QRect(0, 0, width(), height());

    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    std::sort(m_cells.begin(), m_cells.end(), [](const ExpoCell *a, const ExpoCell *b) {
        return a->persistentKey() < b->persistentKey();
    });

    QRect bounds;
    int direction = 0;
    QHash<ExpoCell *, QRect> targets;
    QHash<ExpoCell *, int> directions;

    for (ExpoCell *cell : std::as_const(m_cells)) {
        const QRect cellRect(cell->naturalX(), cell->naturalY(), cell->naturalWidth(), cell->naturalHeight());
        targets[cell] = cellRect;
        // Reuse the unused "slot" as a preferred direction attribute. This is used when the window
        // is on the edge of the screen to try to use as much screen real estate as possible.
        directions[cell] = direction;
        bounds = bounds.united(cellRect);
        direction++;
        if (direction == 4) {
            direction = 0;
        }
    }

    // Iterate over all windows, if two overlap push them apart _slightly_ as we try to
    // brute-force the most optimal positions over many iterations.
    const int halfSpacing = m_spacing / 2;
    bool overlap;
    do {
        overlap = false;
        for (ExpoCell *cell : std::as_const(m_cells)) {
            QRect *target_w = &targets[cell];
            for (ExpoCell *e : std::as_const(m_cells)) {
                if (cell == e) {
                    continue;
                }

                QRect *target_e = &targets[e];
                if (target_w->adjusted(-halfSpacing, -halfSpacing, halfSpacing, halfSpacing)
                        .intersects(target_e->adjusted(-halfSpacing, -halfSpacing, halfSpacing, halfSpacing))) {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff(target_e->center() - target_w->center());
                    // Prevent dividing by zero and non-movement
                    if (diff.x() == 0 && diff.y() == 0) {
                        diff.setX(1);
                    }
                    // Try to keep screen aspect ratio
                    // if (bounds.height() / bounds.width() > area.height() / area.width())
                    //    diff.setY(diff.y() / 2);
                    // else
                    //    diff.setX(diff.x() / 2);
                    // Approximate a vector of between 10px and 20px in magnitude in the same direction
                    diff *= m_accuracy / qreal(diff.manhattanLength());
                    // Move both windows apart
                    target_w->translate(-diff);
                    target_e->translate(diff);

                    // Try to keep the bounding rect the same aspect as the screen so that more
                    // screen real estate is utilised. We do this by splitting the screen into nine
                    // equal sections, if the window center is in any of the corner sections pull the
                    // window towards the outer corner. If it is in any of the other edge sections
                    // alternate between each corner on that edge. We don't want to determine it
                    // randomly as it will not produce consistant locations when using the filter.
                    // Only move one window so we don't cause large amounts of unnecessary zooming
                    // in some situations. We need to do this even when expanding later just in case
                    // all windows are the same size.
                    // (We are using an old bounding rect for this, hopefully it doesn't matter)
                    int xSection = (target_w->x() - bounds.x()) / (bounds.width() / 3);
                    int ySection = (target_w->y() - bounds.y()) / (bounds.height() / 3);
                    diff = QPoint(0, 0);
                    if (xSection != 1 || ySection != 1) { // Remove this if you want the center to pull as well
                        if (xSection == 1) {
                            xSection = (directions[cell] / 2 ? 2 : 0);
                        }
                        if (ySection == 1) {
                            ySection = (directions[cell] % 2 ? 2 : 0);
                        }
                    }
                    if (xSection == 0 && ySection == 0) {
                        diff = QPoint(bounds.topLeft() - target_w->center());
                    }
                    if (xSection == 2 && ySection == 0) {
                        diff = QPoint(bounds.topRight() - target_w->center());
                    }
                    if (xSection == 2 && ySection == 2) {
                        diff = QPoint(bounds.bottomRight() - target_w->center());
                    }
                    if (xSection == 0 && ySection == 2) {
                        diff = QPoint(bounds.bottomLeft() - target_w->center());
                    }
                    if (diff.x() != 0 || diff.y() != 0) {
                        diff *= m_accuracy / qreal(diff.manhattanLength());
                        target_w->translate(diff);
                    }

                    // Update bounding rect
                    bounds = bounds.united(*target_w);
                    bounds = bounds.united(*target_e);
                }
            }
        }
    } while (overlap);

    // Compute the scale factor so the bounding rect fits the target area.
    qreal scale;
    if (bounds.width() <= area.width() && bounds.height() <= area.height()) {
        scale = 1.0;
    } else if (area.width() / qreal(bounds.width()) < area.height() / qreal(bounds.height())) {
        scale = area.width() / qreal(bounds.width());
    } else {
        scale = area.height() / qreal(bounds.height());
    }
    // Make bounding rect fill the screen size for later steps
    bounds = QRect(bounds.x() - (area.width() / scale - bounds.width()) / 2,
                   bounds.y() - (area.height() / scale - bounds.height()) / 2,
                   area.width() / scale,
                   area.height() / scale);

    // Move all windows back onto the screen and set their scale
    QHash<ExpoCell *, QRect>::iterator target = targets.begin();
    while (target != targets.end()) {
        target->setRect((target->x() - bounds.x()) * scale + area.x(),
                        (target->y() - bounds.y()) * scale + area.y(),
                        target->width() * scale,
                        target->height() * scale);
        ++target;
    }

    // Try to fill the gaps by enlarging windows if they have the space
    if (m_fillGaps) {
        // Don't expand onto or over the border
        QRegion borderRegion(area.adjusted(-200, -200, 200, 200));
        borderRegion ^= area;

        bool moved;
        do {
            moved = false;
            for (ExpoCell *cell : std::as_const(m_cells)) {
                QRect oldRect;
                QRect *target = &targets[cell];
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = m_accuracy;
                int heightDiff = heightForWidth(cell, target->width() + widthDiff) - target->height();
                int xDiff = widthDiff / 2; // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.

                // heightDiff (and yDiff) will be re-computed after each successful enlargement attempt
                // so that the error introduced in the window's aspect ratio is minimized

                // Attempt enlarging to the top-right
                oldRect = *target;
                target->setRect(target->x() + xDiff,
                                target->y() - yDiff - heightDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff);
                if (isOverlappingAny(cell, targets, borderRegion, m_spacing)) {
                    *target = oldRect;
                } else {
                    moved = true;
                    heightDiff = heightForWidth(cell, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the bottom-right
                oldRect = *target;
                target->setRect(target->x() + xDiff,
                                target->y() + yDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff);
                if (isOverlappingAny(cell, targets, borderRegion, m_spacing)) {
                    *target = oldRect;
                } else {
                    moved = true;
                    heightDiff = heightForWidth(cell, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the bottom-left
                oldRect = *target;
                target->setRect(target->x() - xDiff - widthDiff,
                                target->y() + yDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff);
                if (isOverlappingAny(cell, targets, borderRegion, m_spacing)) {
                    *target = oldRect;
                } else {
                    moved = true;
                    heightDiff = heightForWidth(cell, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the top-left
                oldRect = *target;
                target->setRect(target->x() - xDiff - widthDiff,
                                target->y() - yDiff - heightDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff);
                if (isOverlappingAny(cell, targets, borderRegion, m_spacing)) {
                    *target = oldRect;
                } else {
                    moved = true;
                }
            }
        } while (moved);

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        for (ExpoCell *cell : std::as_const(m_cells)) {
            QRect *target = &targets[cell];
            qreal scale = target->width() / qreal(cell->naturalWidth());
            if (scale > 2.0 || (scale > 1.0 && (cell->naturalWidth() > 300 || cell->naturalHeight() > 300))) {
                scale = (cell->naturalWidth() > 300 || cell->naturalHeight() > 300) ? 1.0 : 2.0;
                target->setRect(target->center().x() - int(cell->naturalWidth() * scale) / 2,
                                target->center().y() - int(cell->naturalHeight() * scale) / 2,
                                cell->naturalWidth() * scale,
                                cell->naturalHeight() * scale);
            }
        }
    }

    for (ExpoCell *cell : std::as_const(m_cells)) {
        const QRect rect = centered(cell, targets.value(cell).marginsRemoved(cell->margins()));

        cell->setX(rect.x());
        cell->setY(rect.y());
        cell->setWidth(rect.width());
        cell->setHeight(rect.height());
    }
}

void ExpoLayout::resetTransformations()
{
    for (ExpoCell *cell : std::as_const(m_cells)) {
        cell->setX(cell->naturalX());
        cell->setY(cell->naturalY());
        cell->setWidth(cell->naturalWidth());
        cell->setHeight(cell->naturalHeight());
    }
}
