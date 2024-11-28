/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "expolayout.h"

#include <QQmlProperty>
#include <cmath>
#include <deque>
#include <tuple>


ExpoCell::ExpoCell(QQuickItem *parent)
    : QQuickItem(parent)
{
    connect(this, &ExpoCell::visibleChanged, this, [this]() {
        if (m_contentItem) {
            m_contentItem->setVisible(isVisible());
        }
    });

    // This only works for a static visual tree hierarchy.
    // TODO: Make it work with reparenting or warn if any parent in the tree changes?
    QQuickItem *ancestor = this;
    while (ancestor) {
        connect(ancestor, &QQuickItem::xChanged, this, &ExpoCell::polish);
        connect(ancestor, &QQuickItem::yChanged, this, &ExpoCell::polish);
        connect(ancestor, &QQuickItem::widthChanged, this, &ExpoCell::polish);
        connect(ancestor, &QQuickItem::heightChanged, this, &ExpoCell::polish);
        ancestor = ancestor->parentItem();
    }
}

ExpoCell::~ExpoCell()
{
    setLayout(nullptr);
}

void ExpoCell::componentComplete()
{
    QQuickItem::componentComplete();

    QQmlProperty xProperty(this, "Kirigami.ScenePosition.x", qmlContext(this));
    xProperty.connectNotifySignal(this, SLOT(updateContentItemGeometry()));
    QQmlProperty yProperty(this, "Kirigami.ScenePosition.y", qmlContext(this));
    yProperty.connectNotifySignal(this, SLOT(updateContentItemGeometry()));

    updateContentItemGeometry();
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
    if (m_layout && m_shouldLayout) {
        m_layout->addCell(this);
    }
    updateContentItemGeometry();
    Q_EMIT layoutChanged();
}

bool ExpoCell::shouldLayout() const
{
    return m_shouldLayout;
}

void ExpoCell::setShouldLayout(bool shouldLayout)
{
    if (shouldLayout == m_shouldLayout) {
        return;
    }

    m_shouldLayout = shouldLayout;

    if (m_layout) {
        if (m_shouldLayout) {
            m_layout->addCell(this);
        } else {
            m_layout->removeCell(this);
        }
    }

    Q_EMIT shouldLayoutChanged();
}

QQuickItem *ExpoCell::contentItem() const
{
    return m_contentItem;
}

void ExpoCell::setContentItem(QQuickItem *item)
{
    if (m_contentItem == item) {
        return;
    }

    if (m_contentItem) {
        disconnect(m_contentItem, &QQuickItem::parentChanged, this, &ExpoCell::updateContentItemGeometry);
    }

    m_contentItem = item;

    connect(m_contentItem, &QQuickItem::parentChanged, this, &ExpoCell::updateContentItemGeometry);

    if (m_contentItem) {
        m_contentItem->setVisible(isVisible());
    }

    updateContentItemGeometry();
    Q_EMIT contentItemChanged();
}

qreal ExpoCell::partialActivationFactor() const
{
    return m_partialActivationFactor;
}

void ExpoCell::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor == factor) {
        return;
    }

    m_partialActivationFactor = factor;
    // Since this is an animation controller we want it to have immediate effect
    updateContentItemGeometry();

    Q_EMIT partialActivationFactorChanged();
}

void ExpoCell::updateLayout()
{
    if (m_layout) {
        m_layout->polish();
    }
}

qreal ExpoCell::offsetX() const
{
    return m_offsetX;
}

void ExpoCell::setOffsetX(qreal x)
{
    if (m_offsetX != x) {
        m_offsetX = x;
        updateContentItemGeometry();
        Q_EMIT offsetXChanged();
    }
}

qreal ExpoCell::offsetY() const
{
    return m_offsetY;
}

void ExpoCell::setOffsetY(qreal y)
{
    if (m_offsetY != y) {
        m_offsetY = y;
        updateContentItemGeometry();
        Q_EMIT offsetYChanged();
    }
}

qreal ExpoCell::naturalX() const
{
    return m_naturalX;
}

void ExpoCell::setNaturalX(qreal x)
{
    if (m_naturalX != x) {
        m_naturalX = x;
        updateLayout();
        Q_EMIT naturalXChanged();
    }
}

qreal ExpoCell::naturalY() const
{
    return m_naturalY;
}

void ExpoCell::setNaturalY(qreal y)
{
    if (m_naturalY != y) {
        m_naturalY = y;
        updateLayout();
        Q_EMIT naturalYChanged();
    }
}

qreal ExpoCell::naturalWidth() const
{
    return m_naturalWidth;
}

void ExpoCell::setNaturalWidth(qreal width)
{
    if (m_naturalWidth != width) {
        m_naturalWidth = width;
        updateLayout();
        Q_EMIT naturalWidthChanged();
    }
}

qreal ExpoCell::naturalHeight() const
{
    return m_naturalHeight;
}

void ExpoCell::setNaturalHeight(qreal height)
{
    if (m_naturalHeight != height) {
        m_naturalHeight = height;
        updateLayout();
        Q_EMIT naturalHeightChanged();
    }
}

QRectF ExpoCell::naturalRect() const
{
    return QRectF(m_naturalX, m_naturalY, m_naturalWidth, m_naturalHeight);
}

QMarginsF ExpoCell::margins() const
{
    return m_margins;
}

QString ExpoCell::persistentKey() const
{
    return m_persistentKey;
}

void ExpoCell::setPersistentKey(const QString &key)
{
    if (m_persistentKey != key) {
        m_persistentKey = key;
        updateLayout();
        Q_EMIT persistentKeyChanged();
    }
}

qreal ExpoCell::bottomMargin() const
{
    return m_margins.bottom();
}

void ExpoCell::setBottomMargin(qreal margin)
{
    if (m_margins.bottom() != margin) {
        m_margins.setBottom(margin);
        update();
        Q_EMIT bottomMarginChanged();
    }
}

void ExpoCell::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    updateContentItemGeometry();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

void ExpoCell::updateContentItemGeometry()
{
    if (!m_contentItem) {
        return;
    }

    QRectF rect = mapRectToItem(m_contentItem->parentItem(), boundingRect());

    rect = {
        rect.x() * m_partialActivationFactor + (m_naturalX + m_offsetX) * (1.0 - m_partialActivationFactor),
        rect.y() * m_partialActivationFactor + (m_naturalY + m_offsetY) * (1.0 - m_partialActivationFactor),
        rect.width() * m_partialActivationFactor + m_naturalWidth * (1.0 - m_partialActivationFactor),
        rect.height() * m_partialActivationFactor + m_naturalHeight * (1.0 - m_partialActivationFactor)};

    m_contentItem->setX(rect.x());
    m_contentItem->setY(rect.y());
    m_contentItem->setSize(rect.size());
}

ExpoLayout::ExpoLayout(QQuickItem *parent)
    : QQuickItem(parent)
{
}

ExpoLayout::PlacementMode ExpoLayout::placementMode() const
{
    return m_placementMode;
}

void ExpoLayout::setPlacementMode(PlacementMode mode)
{
    if (m_placementMode != mode) {
        m_placementMode = mode;
        polish();
        Q_EMIT placementModeChanged();
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

void ExpoLayout::updateCellsMapping()
{
    for (ExpoCell *cell : m_cells) {
        cell->polish();
    }
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

void ExpoLayout::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.size() != oldGeometry.size()) {
        polish();
    }
    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

// Move and scale rect to fit inside area
static void moveToFit(QRectF &rect, const QRectF &area)
{
    qreal scale = std::min(area.width() / rect.width(), area.height() / rect.height());
    rect.setWidth(rect.width() * scale);
    rect.setHeight(rect.height() * scale);
    rect.moveCenter(area.center());
}

void ExpoLayout::updatePolish()
{
    if (m_cells.isEmpty()) {
        setReady();
        return;
    }

    QRectF area = QRectF(0, 0, width(), height());

    std::sort(m_cells.begin(), m_cells.end(),
              [](const ExpoCell *a, const ExpoCell *b) {
                  return a->persistentKey() < b->persistentKey();
              });

    // Estimate the scale factor we need to apply by simple heuristics
    qreal totalArea = 0;
    qreal availableArea = area.width() * area.height();
    for (ExpoCell *cell : std::as_const(m_cells)) {
        totalArea += cell->naturalWidth() * cell->naturalHeight();
    }
    qreal scale = std::sqrt(availableArea / totalArea) * 0.7; // conservative estimate
    scale = std::clamp(scale, 0.1, 10.0); // don't go crazy

    QList<QRectF> windowSizes;
    for (ExpoCell *cell : std::as_const(m_cells)) {
        const QMarginsF &margins = cell->margins();
        const QMarginsF scaledMargins(margins.left() / scale, margins.top() / scale, margins.right() / scale, margins.bottom() / scale);
        windowSizes.emplace_back(cell->naturalRect().marginsAdded(scaledMargins));
    }
    auto windowLayouts = ExpoLayout::layout(area, windowSizes);
    for (int i = 0; i < windowLayouts.size(); ++i) {
        ExpoCell *cell = m_cells[i];
        QRectF target = windowLayouts[i];

        QRectF adjustedTarget = target.marginsRemoved(cell->margins());
        if (adjustedTarget.isValid()) {
            target = adjustedTarget; // Borders
        }

        QRectF rect = cell->naturalRect();
        moveToFit(rect, target);
        if (m_ready) {
            // Use setProperty so the QML side can animate with Behavior
            cell->setProperty("x", rect.x());
            cell->setProperty("y", rect.y());
            cell->setProperty("width", rect.width());
            cell->setProperty("height", rect.height());
        } else {
            cell->setX(rect.x());
            cell->setY(rect.y());
            cell->setWidth(rect.width());
            cell->setHeight(rect.height());
        }
    }
    setReady();
}

Layer::Layer(qreal maxWidth, const QList<QRectF> &windowSizes, const QList<size_t> &windowIds, size_t startPos, size_t endPos)
    : maxWidth(maxWidth)
    , maxHeight(windowSizes[windowIds[endPos - 1]].height())
    , ids(windowIds.begin() + startPos, windowIds.begin() + endPos)
{
    remainingWidth = maxWidth;
    for (auto id = ids.begin(); id != ids.end(); ++id) {
        remainingWidth -= windowSizes[*id].width();
    }
}

qreal Layer::width() const
{
    return maxWidth - remainingWidth;
}

LayeredPacking::LayeredPacking(qreal maxWidth, const QList<QRectF> &windowSizes, const QList<size_t> &ids, const QList<size_t> &layerStartPos)
    : maxWidth(maxWidth)
    , width(0)
    , height(0)
{
    for (int i = 1; i < layerStartPos.size(); ++i) {
        layers.emplace_back(maxWidth, windowSizes, ids, layerStartPos[i - 1], layerStartPos[i]);
        width = std::max(width, layers.back().width());
        height += layers.back().maxHeight;
    }
}

/**
 * @brief Check if @param candidate can be ignored in the future because either @param alternativeSmall or @param alternativeBig is at least as good as @param candidate for layerStart.
 *
 * More formally, returns false if and only if there exists a k with @param alternativeBig < k <= @param length
 * such that leastWeightCandidate( @param candidate, k ) < leastWeightCandidate( @param alternativeSmall, k ) and leastWeightCandidate( @param candidate, k ) < leastWeightCandidate(
 * @param alternativeBig, k ).
 *
 * The input must satisfy @param alternativeSmall < @param candidate < @param alternativeBig
 *
 * The run time of the algorithm is O(log length).
 *
 * The Bridge algorithm from Hirschberg, Daniel S., and Lawrence L.
 * Larmore. "The least weight subsequence problem." SIAM Journal on
 * Computing 16.4 (1987): 628-638
 *
 * @param length The length of the sequence.
 * @param leastWeightCandidate leastWeightCandidate(i, j) is the weight of arranging the first j windows,
 * if we use the optimal arrangement of the first i windows, and the last layest consists of windows [i, j)
 */
static bool isDominated(size_t candidate, size_t alternativeSmall, size_t alternativeBig, size_t length, std::function<qreal(size_t, size_t)> leastWeightCandidate)
{
    Q_ASSERT(alternativeSmall < candidate && candidate < alternativeBig);
    if (alternativeBig == length) {
        return true;
    }

    // We assumed that the weigth function is concave, i.e., for all i <= j < k <= l,
    //     weight(i,k) + weight(j,l) <= weight(i,l) + weight(j,k)
    // This implies the following about leastWeightCandidate:
    // For all i <= j < k <= l
    // - If leastWeightCandidate(i, l) <= leastWeightCandidate(j, l), then leastWeightCandidate(i, k) <= leastWeightCandidate(j, k)
    // - If leastWeightCandidate(j, k) <= leastWeightCandidate(i, k), then leastWeightCandidate(j, l) <= leastWeightCandidate(i, l)
    //
    // In particular, this implies that the set of ks such that
    //     leastWeightCandidate(candidate, k) < leastWeightCandidate(alternativeSmall, k)
    // is a (possibly empty) interval [k1, length] for some k1.
    // This is because if for some k,
    //     leastWeightCandidate(alternativeSmall, k) <= leastWeightCandidate(candidate, k),
    // then for all candidate < k' <= k,
    //     leastWeightCandidate(alternativeSmall, k') <= leastWeightCandidate(candidate, k')
    //
    // Similarly, the set of ks such that
    //     leastWeightCandidate(candidate, k) < leastWeightCandidate(alternativeBig, k)
    // is a (possibly empty) interval [alternativeBig + 1, k2] for some k2.
    // This is because if for some k,
    //     leastWeightCandidate(alternativeBig, k) <= leastWeightCandidate(candidate, k),
    // then for all k' >= k
    //     leastWeightCandidate(alternativeBig, k') <= leastWeightCandidate(candidate, k')
    //
    // Hence, to check if a k exists in both intervals, we can use binary search to find the smallest k1 such that
    //     leastWeightCandidate(candidate, k1) < leastWeightCandidate(alternativeSmall, k1).
    // If such a k1 exists, it suffices to check if
    //     leastWeightCandidate(alternativeBig, k1) <= leastWeightCandidate(candidate, k1).
    if (leastWeightCandidate(alternativeSmall, length) <= leastWeightCandidate(candidate, length)) {
        return true;
    }
    // Now we know that leastWeightCandidate(candidate, length) < leastWeightCandidate(alternativeSmall, length)
    // i.e, the first interval is non-empty
    // Our candidate k1 is in the interval (low, high] (inclusive on high)
    size_t low = alternativeBig;
    size_t high = length;
    while (high - low >= 2) {
        size_t mid = (low + high) / 2;
        if (leastWeightCandidate(alternativeSmall, mid) <= leastWeightCandidate(candidate, mid)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return (leastWeightCandidate(alternativeBig, high) <= leastWeightCandidate(candidate, high));
}

/**
 * @brief Returns the layerStartPos for a good packing of the windows using the Basic algorithm
 * from Hirschberg, Daniel S., and Lawrence L. Larmore. "The least weight subsequence problem."
 * SIAM Journal on Computing 16.4 (1987): 628-638.
 *
 * The Basic algorithm solves the Least Weight Subsequence Problem (LWS) for
 * concave weight functions.
 *
 * The LWS problem on the interval [a,b] is defined as follows:
 * Given a weight function weight(i,j) for all i,j in [a,b], find a subsequence
 * of [a,b], i.e. a sequence of strictly monotonically increasing indices
 * i_0 < i_2 < ... < i_t, such that the total weight,
 * sum_{k=1}^t weight(i_{k-1}, i_k), is minimized.
 *
 * A weight function is concave if for all i <= j < k <= l, the following holds:
 * weight(i,k) + weight(j,l) <= weight(i,l) + weight(j,k)
 *
 * The run time of the algorithm is O(n log n).
 *
 * Modified from the version in the paper to fix some bugs.
 *
 * @param idealWidth The target width of each layer. All widths of windows *MUST* be smaller than idealWidth.
 * @param length The length of the sequence. Solves the LWS problem on the interval [0, length]. (n in paper)
 * @param cumWidths cumWidths[i] is the sum of widths of windows 0, 1, ..., i - 1
 *
 * @return QList<size_t> The subsequence (starting at 0 and ending at length)
 * that minimizes the total weight. The ith element is the index of the first window in layer i.
 * Always starts with 0 and ends with ids.size().
 */
static QList<size_t> getLayerStartPos(qreal maxWidth, qreal idealWidth, const size_t length, const QList<qreal> &cumWidths)
{
    // weight(start, end) is the penalty of placing all windows in the range [start, end) in the same layer.
    // The following form only works when the maximum width of a window is less than or equal to idealWidth.
    //
    // The weight function is designed such that
    // 1. The weight function is concave (see definition in Basic algorithm)
    // 2. It scales like (width - idealWidth) ^ 2 for width < idealWidth
    // 3. Exceeding maxWidth is guaranteed to be worse than any other solution
    //
    // 1. holds as long as weight(i, j) = f(cumWidths[j] - cumWidths[i]) for some convex function f
    // 3. is guaranteed by making the penalty of exceeding maxWidth at least
    // cumWidths.size(), which strictly upper bounds the total weight of placing
    // each window in its own layer
    //
    auto weight = [maxWidth, idealWidth, &cumWidths](size_t start, size_t end) {
        qreal width = cumWidths[end] - cumWidths[start];
        if (width < idealWidth) {
            return (width - idealWidth) * (width - idealWidth) / idealWidth / idealWidth;
        } else {
            qreal penaltyFactor = cumWidths.size();
            return penaltyFactor * (width - idealWidth) * (width - idealWidth) / (maxWidth - idealWidth) / (maxWidth - idealWidth);
        }
    };

    // layerStart[j] is where the last layer should start, if there were only the first j windows.
    // I.e., layerStart[5]=3 means that the last layer should start at window 3 if there were only the first 10 windows
    // (bestLeft in paper)
    QList<size_t> layerStart(length + 1);
    // leastWeight[i] is the least weight of any subsequence starting at 0 and ending at i (f in paper)
    QList<qreal> leastWeight(length + 1);
    // layerStartcandidates contains all current candidates for layerStart[currentIndex] (d in paper)
    std::deque<size_t> layerStartCandidates;

    leastWeight[0] = 0;

    // leastWeightCandidate(lastRowStartPos, num) is a candidate value for leastWeight[num].
    // It is the weight for arranging the first num windows, assuming optimal arrangement of
    // the first lastRowStartPos windows, and a last layer consisting of windows [lastRowStartPos, num)
    // (g in paper)
    auto leastWeightCandidate = [&leastWeight, &weight](size_t lastRowStartPos, size_t num) {
        return leastWeight[lastRowStartPos] + weight(lastRowStartPos, num);
    };
    layerStartCandidates.push_back(0);
    for (size_t currentIndex = 1; currentIndex < length; ++currentIndex) { // currentIndex is m in paper
        leastWeight[currentIndex] = leastWeightCandidate(layerStartCandidates.front(), currentIndex);
        layerStart[currentIndex] = layerStartCandidates.front();

        // Modification of algorithm in paper;
        // needed so that layerStartCandidates.front can be correctly removed when layerStartCandidates.size() == 1
        layerStartCandidates.push_back(currentIndex);

        // Remove candidates from the front if they are dominated by the second candidate
        // Dominate means that the second candidate is at least as good as the first candidate for layerStart
        while (layerStartCandidates.size() >= 2 && leastWeightCandidate(layerStartCandidates[1], currentIndex + 1) <= leastWeightCandidate(layerStartCandidates[0], currentIndex + 1)) {
            layerStartCandidates.pop_front();
        }
        layerStartCandidates.pop_back(); // Modification of algorithm in paper

        // Remove candidates from the back if they are dominated by either the second to last candidate, or currentIndex
        while (layerStartCandidates.size() >= 2 && isDominated(layerStartCandidates.back(), layerStartCandidates[layerStartCandidates.size() - 2], currentIndex, length, leastWeightCandidate)) {
            layerStartCandidates.pop_back();
        }

        // Modification of algorithm in paper; we need at least one candidate in layerStartCandidates
        if (layerStartCandidates.empty()) {
            layerStartCandidates.push_back(currentIndex);
            continue;
        }

        // Add currentIndex to layerStartCandidates if it is not dominated by the last candidate
        if (leastWeightCandidate(currentIndex, length) < leastWeightCandidate(layerStartCandidates.back(), length)) {
            layerStartCandidates.push_back(currentIndex);
        }
    }

    // recover the solution using layerStart
    leastWeight[length] = leastWeightCandidate(layerStartCandidates.front(), length);
    layerStart[length] = layerStartCandidates.front();

    QList<size_t> layerStartPosReversed;
    layerStartPosReversed.push_back(length);
    size_t currentIndex = length;

    while (currentIndex > 0) {
        currentIndex = layerStart[currentIndex];
        layerStartPosReversed.push_back(currentIndex);
    }

    return QList<size_t>(layerStartPosReversed.rbegin(), layerStartPosReversed.rend());
}

// Reflection about the line y = x
static QMarginsF reflect(const QMarginsF &margins)
{
    return QMarginsF(margins.top(), margins.right(), margins.bottom(), margins.left());
}
static QRectF reflect(const QRectF &rect)
{
    return QRectF(rect.y(), rect.x(), rect.height(), rect.width());
}
static QPointF reflect(const QPointF &point)
{
    return point.transposed();
}
template<typename T>
static QList<T> reflect(const QList<T> &v)
{
    QList<T> result;
    result.reserve(v.size());
    for (const auto &x : v) {
        result.emplace_back(reflect(x));
    }
    return result;
}

QList<QRectF> ExpoLayout::layout(const QRectF &area, const QList<QRectF> &windowSizes)
{
    const qreal shortSide = std::min(area.width(), area.height());
    const QMarginsF margins(shortSide * m_relativeMarginLeft,
                            shortSide * m_relativeMarginTop,
                            shortSide * m_relativeMarginRight,
                            shortSide * m_relativeMarginBottom);
    const qreal minLength = m_relativeMinLength * shortSide;
    const QRectF minSize = QRectF(0, 0, minLength, minLength);

    QList<QPointF> centers;
    for (const QRectF &windowSize : windowSizes) {
        centers.push_back(windowSize.center());
    }

    // windows bigger than 4x the area are considered ill-behaved and their sizes are clipped
    const auto adjustedSizes = adjustSizes(minSize, QRectF(0, 0, 4 * area.width(), 4 * area.height()), margins, windowSizes);

    if (placementMode() == PlacementMode::Rows) {
        LayeredPacking bestPacking = findGoodPacking(area, adjustedSizes, centers, m_idealWidthRatio, m_searchTolerance);
        return refineAndApplyPacking(area, margins, bestPacking, adjustedSizes, centers);
    } else {
        QList<QRectF> adjustedSizesReflected(reflect(adjustedSizes));
        QList<QPointF> centersReflected(reflect(centers));

        LayeredPacking bestPacking = findGoodPacking(area.transposed(), adjustedSizesReflected, centersReflected, m_idealWidthRatio, m_searchTolerance);
        return reflect(refineAndApplyPacking(area.transposed(), reflect(margins), bestPacking, adjustedSizesReflected, centersReflected));
    }
}

QList<QRectF> ExpoLayout::adjustSizes(const QRectF &minSize, const QRectF &maxSize, const QMarginsF &margins, const QList<QRectF> &windowSizes)
{
    QList<QRectF> adjustedSizes;
    for (QRectF windowSize : windowSizes) {
        windowSize.setWidth(std::clamp(windowSize.width(), minSize.width(), maxSize.width()));
        windowSize.setHeight(std::clamp(windowSize.height(), minSize.height(), maxSize.height()));
        windowSize += margins;
        adjustedSizes.emplace_back(windowSize);
    }
    return adjustedSizes;
}

LayeredPacking
ExpoLayout::findGoodPacking(const QRectF &area, const QList<QRectF> &windowSizes, const QList<QPointF> &centers, qreal idealWidthRatio, qreal tol)
{
    QList<std::tuple<size_t, QRectF, QPointF>> windowSizesWithIds;

    for (int i = 0; i < windowSizes.size(); ++i) {
        windowSizesWithIds.emplace_back(i, windowSizes[i], centers[i]);
    }

    // Sorting by height ensures that windows in same layer (row) have similar heights
    std::stable_sort(windowSizesWithIds.begin(), windowSizesWithIds.end(), [](const auto &a, const auto &b) {
        // in case of same height, sort by y to minimize vertical movement
        return std::tuple(std::get<1>(a).height(), std::get<2>(a).y())
            < std::tuple(std::get<1>(b).height(), std::get<2>(b).y());
    });

    QList<size_t> ids; // ids of windows in sorted order
    QList<qreal> cumWidths; // cumWidths[i] is the sum of widths of windows 0, 1, ..., i - 1

    // Minimum and maximum strip widths to use in the binary search.
    // Strips should be at least as wide as the widest window, and at most as
    // wide as the sum of all window widths.
    qreal stripWidthMin = 0;
    qreal stripWidthMax = 0;

    cumWidths.push_back(0);
    for (const auto &windowSizeWithId : windowSizesWithIds) {
        ids.push_back(std::get<0>(windowSizeWithId));
        qreal width = std::get<1>(windowSizeWithId).width();
        cumWidths.push_back(cumWidths.back() + width);

        stripWidthMin = std::max(stripWidthMin, width);
        stripWidthMax += width;
    }
    stripWidthMin /= idealWidthRatio;
    stripWidthMax /= idealWidthRatio;

    qreal targetRatio = area.height() / area.width();

    auto findPacking = [&windowSizes, &ids, &cumWidths, idealWidthRatio](qreal stripWidth) {
        QList<size_t> layerStartPos = getLayerStartPos(stripWidth, stripWidth * idealWidthRatio, ids.size(), cumWidths);
        LayeredPacking result(stripWidth, windowSizes, ids, layerStartPos);
        Q_ASSERT(result.width <= stripWidth);
        return result;
    };

    // the placement with the minimum strip width corresponds with a big aspect
    // ratio (ratioHigh), and the placement with the maximum strip width
    // corresponds with a small aspect ratio (ratioLow)

    LayeredPacking placementWidthMin = findPacking(stripWidthMin);
    qreal ratioHigh = placementWidthMin.height / placementWidthMin.width;

    if (ratioHigh <= targetRatio) {
        return placementWidthMin;
    }

    LayeredPacking placementWidthMax = findPacking(stripWidthMax);
    qreal ratioLow = placementWidthMax.height / placementWidthMax.width;

    if (ratioLow >= targetRatio) {
        return placementWidthMax;
    }

    while (stripWidthMax / stripWidthMin > 1 + tol) {
        qreal stripWidthMid = std::sqrt(stripWidthMin * stripWidthMax);
        LayeredPacking placementMid = findPacking(stripWidthMid);
        qreal ratioMid = placementMid.height / placementMid.width;

        if (ratioMid > targetRatio) {
            stripWidthMin = stripWidthMid;
            placementWidthMin = placementMid;
            ratioHigh = ratioMid;
        } else {
            // small optimization: use the actual strip width
            stripWidthMax = placementMid.width;
            placementWidthMax = placementMid;
            ratioLow = ratioMid;
        }
    }

    // how much we need to scale the placement to fit
    qreal scaleWidthMin = std::min(area.width() / placementWidthMin.width, area.height() / placementWidthMin.height);
    qreal scaleWidthMax = std::min(area.width() / placementWidthMax.width, area.height() / placementWidthMax.height);

    if (scaleWidthMin > scaleWidthMax) {
        return placementWidthMin;
    } else {
        return placementWidthMax;
    }
}

QList<QRectF> ExpoLayout::refineAndApplyPacking(const QRectF &area, const QMarginsF &margins, const LayeredPacking &packing, const QList<QRectF> &windowSizes, const QList<QPointF> &centers)
{
    // Scale packing to fit area
    qreal scale = std::min(area.width() / packing.width, area.height() / packing.height);
    scale = std::min(scale, m_maxScale);

    const QMarginsF scaledMargins = QMarginsF(margins.left() * scale, margins.top() * scale,
                                              margins.right() * scale, margins.bottom() * scale);

    // The maximum gap in additional to margins to leave between windows
    qreal maxGapY = m_maxGapRatio * (scaledMargins.top() + scaledMargins.bottom());
    qreal maxGapX = m_maxGapRatio * (scaledMargins.left() + scaledMargins.right());

    // center align y
    qreal extraY = area.height() - packing.height * scale;
    qreal gapY = std::min(maxGapY, extraY / (packing.layers.size() + 1));
    qreal y = area.y() + (extraY - gapY * (packing.layers.size() - 1)) / 2;

    QList<QRectF> finalWindowLayouts(windowSizes);
    // smaller windows "float" to the top
    for (const auto &layer : packing.layers) {
        qreal extraX = area.width() - layer.width() * scale;
        qreal gapX = std::min(maxGapX, extraX / (layer.ids.size() + 1));
        qreal x = area.x() + (extraX - gapX * (layer.ids.size() - 1)) / 2;

        QList<size_t> ids(layer.ids);
        std::stable_sort(ids.begin(), ids.end(), [&centers](size_t a, size_t b) {
            return centers[a].x() < centers[b].x(); // minimize horizontal movement
        });
        for (auto id : std::as_const(ids)) {
            QRectF &windowLayout = finalWindowLayouts[id];
            qreal newY = y + (layer.maxHeight - windowLayout.height()) * scale / 2; // center align y
            windowLayout = QRectF(x, newY, windowLayout.width() * scale, windowLayout.height() * scale);
            x += windowLayout.width() + gapX;
            windowLayout -= scaledMargins;
        }
        y += layer.maxHeight * scale + gapY;
    }
    return finalWindowLayouts;
}

#include "moc_expolayout.cpp"
