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
    if (m_layout) {
        m_layout->addCell(this);
    }
    Q_EMIT layoutChanged();
}

void ExpoCell::update()
{
    if (m_layout) {
        m_layout->scheduleUpdate();
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

        if (!m_x.has_value()) {
            m_x = x;
            Q_EMIT xChanged();
        }
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

        if (!m_y.has_value()) {
            m_y = y;
            Q_EMIT yChanged();
        }
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

        if (!m_width.has_value()) {
            m_width = width;
            Q_EMIT widthChanged();
        }
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

        if (!m_height.has_value()) {
            m_height = height;
            Q_EMIT heightChanged();
        }
    }
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

ExpoLayout::ExpoLayout(QQuickItem *parent)
    : QQuickItem(parent)
{
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &ExpoLayout::update);
}

ExpoLayout::LayoutMode ExpoLayout::mode() const
{
    return m_mode;
}

void ExpoLayout::setMode(LayoutMode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        scheduleUpdate();
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
        scheduleUpdate();
        Q_EMIT fillGapsChanged();
    }
}

void ExpoLayout::update()
{
    if (m_cells.isEmpty()) {
        return;
    }
    switch (m_mode) {
    case LayoutClosest:
        calculateWindowTransformationsClosest();
        break;
    case LayoutKompose:
        calculateWindowTransformationsKompose();
        break;
    case LayoutNatural:
        calculateWindowTransformationsNatural();
        break;
    }
}

void ExpoLayout::scheduleUpdate()
{
    m_updateTimer.start();
}

void ExpoLayout::addCell(ExpoCell *cell)
{
    Q_ASSERT(!m_cells.contains(cell));
    m_cells.append(cell);
    scheduleUpdate();
}

void ExpoLayout::removeCell(ExpoCell *cell)
{
    m_cells.removeOne(cell);
    scheduleUpdate();
}

void ExpoLayout::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.size() != oldGeometry.size()) {
        scheduleUpdate();
    }
}

static int distance(const QPoint &a, const QPoint &b)
{
    const int xdiff = a.x() - b.x();
    const int ydiff = a.y() - b.y();
    return int(std::sqrt(qreal(xdiff * xdiff + ydiff * ydiff)));
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
    takenSlots.fill(0);

    // precalculate all slot centers
    QVector<QPoint> slotCenters;
    slotCenters.resize(rows * columns);
    for (int x = 0; x < columns; ++x)
        for (int y = 0; y < rows; ++y) {
            slotCenters[x + y * columns] = QPoint(area.x() + slotWidth * x + slotWidth / 2,
                                                  area.y() + slotHeight * y + slotHeight / 2);
        }

    // Assign each window to the closest available slot
    QList<ExpoCell *> tmpList = m_cells; // use a QLinkedList copy instead?
    while (!tmpList.isEmpty()) {
        ExpoCell *cell = tmpList.first();
        int slotCandidate = -1, slotCandidateDistance = INT_MAX;
        const QPoint pos(cell->naturalX() + cell->naturalWidth() / 2,
                         cell->naturalY() + cell->naturalHeight() / 2);

        for (int i = 0; i < columns*rows; ++i) { // all slots
            const int dist = distance(pos, slotCenters[i]);
            if (dist < slotCandidateDistance) { // window is interested in this slot
                ExpoCell *occupier = takenSlots[i];
                Q_ASSERT(occupier != cell);
                const QPoint occupierPos(occupier->naturalX() + occupier->naturalWidth() / 2,
                                         occupier->naturalY() + occupier->naturalHeight() / 2);
                if (!occupier || dist < distance(occupierPos, slotCenters[i])) {
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
        target.adjust(10, 10, -10, -10);   // Borders

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

static inline qreal aspectRatio(ExpoCell *cell)
{
    return cell->naturalWidth() / qreal(cell->naturalHeight());
}

static inline int widthForHeight(ExpoCell *cell, int height)
{
    return int((height / qreal(cell->naturalHeight())) * cell->naturalWidth());
}

static inline int heightForWidth(ExpoCell *cell, int width)
{
    return int((width / qreal(cell->naturalWidth())) * cell->naturalHeight());
}

void ExpoLayout::calculateWindowTransformationsKompose()
{
    const QRect availRect = QRect(0, 0, width(), height());
    std::sort(m_cells.begin(), m_cells.end(), [](const ExpoCell *a, const ExpoCell *b) {
        return a->persistentKey() < b->persistentKey();
    });   // The location of the windows should not depend on the stacking order

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp
    int spacing = 10;
    int rows, columns;
    qreal parentRatio = availRect.width() / qreal(availRect.height());
    // Use more columns than rows when parent's width > parent's height
    if (parentRatio > 1) {
        columns = std::ceil(std::sqrt(qreal(m_cells.count())));
        rows = std::ceil(qreal(m_cells.count()) / columns);
    } else {
        rows = std::ceil(sqrt(qreal(m_cells.count())));
        columns = std::ceil(qreal(m_cells.count()) / rows);
    }
    //qCDebug(KWINEFFECTS) << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients";

    // Calculate width & height
    int w = (availRect.width() - (columns + 1) * spacing) / columns;
    int h = (availRect.height() - (rows + 1) * spacing) / rows;

    QList<ExpoCell *>::iterator it(m_cells.begin());
    QList<QRect> geometryRects;
    QList<int> maxRowHeights;
    // Process rows
    for (int i = 0; i < rows; ++i) {
        int xOffsetFromLastCol = 0;
        int maxHeightInRow = 0;
        // Process columns
        for (int j = 0; j < columns; ++j) {
            ExpoCell *cell;

            // Check for end of List
            if (it == m_cells.end()) {
                break;
            }
            cell = *it;

            // Calculate width and height of widget
            qreal ratio = aspectRatio(cell);

            int widgetWidth = 100;
            int widgetHeight = 100;
            int usableWidth = w;
            int usableHeight = h;

            // use width of two boxes if there is no right neighbour
            if (cell == m_cells.last() && j != columns - 1) {
                usableWidth = 2 * w;
            }
            ++it; // We need access to the neighbour in the following
            // expand if right neighbour has ratio < 1
            if (j != columns - 1 && it != m_cells.end() && aspectRatio(*it) < 1) {
                int addW = w - widthForHeight(*it, h);
                if (addW > 0) {
                    usableWidth = w + addW;
                }
            }

            if (ratio == -1) {
                widgetWidth = w;
                widgetHeight = h;
            } else {
                qreal widthByHeight = widthForHeight(cell, usableHeight);
                qreal heightByWidth = heightForWidth(cell, usableWidth);
                if ((ratio >= 1.0 && heightByWidth <= usableHeight) ||
                        (ratio < 1.0 && widthByHeight > usableWidth)) {
                    widgetWidth = usableWidth;
                    widgetHeight = (int)heightByWidth;
                } else if ((ratio < 1.0 && widthByHeight <= usableWidth) ||
                           (ratio >= 1.0 && heightByWidth > usableHeight)) {
                    widgetHeight = usableHeight;
                    widgetWidth = (int)widthByHeight;
                }
                // Don't upscale large-ish windows
                if (widgetWidth > cell->naturalWidth() && (cell->naturalWidth() > 300 || cell->naturalHeight() > 300)) {
                    widgetWidth = cell->naturalWidth();
                    widgetHeight = cell->naturalHeight();
                }
            }

            // Set the Widget's size

            int alignmentXoffset = 0;
            int alignmentYoffset = 0;
            if (i == 0 && h > widgetHeight) {
                alignmentYoffset = h - widgetHeight;
            }
            if (j == 0 && w > widgetWidth) {
                alignmentXoffset = w - widgetWidth;
            }
            QRect geom(availRect.x() + j *(w + spacing) + spacing + alignmentXoffset + xOffsetFromLastCol,
                       availRect.y() + i *(h + spacing) + spacing + alignmentYoffset,
                       widgetWidth, widgetHeight);
            geometryRects.append(geom);

            // Set the x offset for the next column
            if (alignmentXoffset == 0) {
                xOffsetFromLastCol += widgetWidth - w;
            }
            if (maxHeightInRow < widgetHeight) {
                maxHeightInRow = widgetHeight;
            }
        }
        maxRowHeights.append(maxHeightInRow);
    }

    int topOffset = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            const int pos = i * columns + j;
            if (pos >= m_cells.count()) {
                break;
            }

            ExpoCell *cell = m_cells[pos];
            QRect target = geometryRects[pos];
            target.setY(target.y() + topOffset);
            // @Marrtin: any idea what this is good for?
//             DataHash::iterator winData = m_windowData.find(window);
//             if (winData != m_windowData.end())
//                 winData->slot = pos;

            cell->setX(target.x());
            cell->setY(target.y());
            cell->setWidth(target.width());
            cell->setHeight(target.height());
        }
        if (maxRowHeights[i] - h > 0) {
            topOffset += maxRowHeights[i] - h;
        }
    }
}

static bool isOverlappingAny(ExpoCell *w, const QHash<ExpoCell *, QRect> &targets, const QRegion &border)
{
    QHash<ExpoCell *, QRect>::const_iterator winTarget = targets.find(w);
    if (winTarget == targets.constEnd()) {
        return false;
    }
    if (border.intersects(*winTarget)) {
        return true;
    }

    // Is there a better way to do this?
    QHash<ExpoCell *, QRect>::const_iterator target;
    for (target = targets.constBegin(); target != targets.constEnd(); ++target) {
        if (target == winTarget) {
            continue;
        }
        if (winTarget->adjusted(-5, -5, 5, 5).intersects(target->adjusted(-5, -5, 5, 5))) {
            return true;
        }
    }
    return false;
}

void ExpoLayout::calculateWindowTransformationsNatural()
{
    QRect area = QRect(0, 0, width(), height());
    if (m_cells.count() == 1) {
        // Just move the window to its original location to save time
        ExpoCell *cell = m_cells.constFirst();
        if (area.contains(QRect(cell->naturalX(), cell->naturalY(), cell->naturalWidth(), cell->naturalHeight()))) {
            cell->setX(cell->naturalX());
            cell->setY(cell->naturalY());
            cell->setWidth(cell->naturalWidth());
            cell->setHeight(cell->naturalHeight());
            return;
        }
    }

    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    std::sort(m_cells.begin(), m_cells.end(), [](const ExpoCell *a, const ExpoCell *b) {
        return a->persistentKey() < b->persistentKey();
    });

    QRect bounds = area;
    int direction = 0;
    QHash<ExpoCell *, QRect> targets;
    QHash<ExpoCell *, int> directions;

    for (ExpoCell *cell : qAsConst(m_cells)) {
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
    bool overlap;
    do {
        overlap = false;
        for (ExpoCell *cell : qAsConst(m_cells)) {
            QRect *target_w = &targets[cell];
            for (ExpoCell *e : qAsConst(m_cells)) {
                if (cell == e) {
                    continue;
                }

                QRect *target_e = &targets[e];
                if (target_w->adjusted(-5, -5, 5, 5).intersects(target_e->adjusted(-5, -5, 5, 5))) {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff(target_e->center() - target_w->center());
                    // Prevent dividing by zero and non-movement
                    if (diff.x() == 0 && diff.y() == 0) {
                        diff.setX(1);
                    }
                    // Try to keep screen aspect ratio
                    //if (bounds.height() / bounds.width() > area.height() / area.width())
                    //    diff.setY(diff.y() / 2);
                    //else
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

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    // The 20's and 10's are so that the windows don't touch the edge of the screen.
    qreal scale;
    if (bounds == area) {
        scale = 1.0; // Don't add borders to the screen
    } else if (area.width() / qreal(bounds.width()) < area.height() / qreal(bounds.height())) {
        scale = (area.width() - 20) / qreal(bounds.width());
    } else {
        scale = (area.height() - 20) / qreal(bounds.height());
    }
    // Make bounding rect fill the screen size for later steps
    bounds = QRect((bounds.x() * scale - (area.width() - 20 - bounds.width() * scale) / 2 - 10) / scale,
                   (bounds.y() * scale - (area.height() - 20 - bounds.height() * scale) / 2 - 10) / scale,
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
        borderRegion ^= area.adjusted(10 / scale, 10 / scale, -10 / scale, -10 / scale);

        bool moved;
        do {
            moved = false;
            for (ExpoCell *cell : qAsConst(m_cells)) {
                QRect oldRect;
                QRect *target = &targets[cell];
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = m_accuracy;
                int heightDiff = heightForWidth(cell, target->width() + widthDiff) - target->height();
                int xDiff = widthDiff / 2;  // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.

                // heightDiff (and yDiff) will be re-computed after each successful enlargement attempt
                // so that the error introduced in the window's aspect ratio is minimized

                // Attempt enlarging to the top-right
                oldRect = *target;
                target->setRect(target->x() + xDiff,
                                target->y() - yDiff - heightDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff);
                if (isOverlappingAny(cell, targets, borderRegion))
                    *target = oldRect;
                else {
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
                if (isOverlappingAny(cell, targets, borderRegion))
                    *target = oldRect;
                else {
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
                if (isOverlappingAny(cell, targets, borderRegion))
                    *target = oldRect;
                else {
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
                if (isOverlappingAny(cell, targets, borderRegion)) {
                    *target = oldRect;
                } else {
                    moved = true;
                }
            }
        } while (moved);

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        for (ExpoCell *cell : qAsConst(m_cells)) {
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

    for (ExpoCell *cell : qAsConst(m_cells)) {
        const QRect rect = targets.value(cell);

        cell->setX(rect.x());
        cell->setY(rect.y());
        cell->setWidth(rect.width());
        cell->setHeight(rect.height());
    }
}
