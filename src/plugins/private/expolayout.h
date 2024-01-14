/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QObject>
#include <QQuickItem>
#include <QRect>

#include <optional>

class ExpoCell;
struct Layer;
struct LayeredPacking;

/**
 * @brief Adapts the algorithm from [0] to layout the windows intelligently.
 *
 * Design goals:
 * - use screen space efficiently, given diverse geometries of windows
 * - be aesthetically pleasing
 * - and minimize movement of windows from initial positions
 *
 * More concretely, the algorithm produces a layered layout, where each layer,
 * or strip, is a row or column. The algorithm tries to ensure that different
 * strips have similar widths, and uses binary search to find a packing with
 * similar aspect ratio to the layout area. Within each strip, the algorithm
 * tries to minimize horizontal movement (for rows) or vertical movement (for
 * columns) of the windows.
 *
 * [0] Hirschberg, Daniel S., and Lawrence L. Larmore. "The least weight
 * subsequence problem." SIAM Journal on Computing 16.4 (1987): 628-638.
 */
class ExpoLayout : public QQuickItem
{
    Q_OBJECT
    // Place windows in rows or columns.
    Q_PROPERTY(PlacementMode placementMode READ placementMode WRITE setPlacementMode NOTIFY placementModeChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    /**
     * Stop binary search when the two candidate strip widths are within tol (as a fraction of the larger strip width).
     * Default is 0.2.
     */
    Q_PROPERTY(qreal searchTolerance MEMBER m_searchTolerance NOTIFY searchToleranceChanged)
    /**
     * The ideal sum of window widths in a strip (including added margins), as a fraction of the strip width. *MUST* be strictly less than 1.
     * Default is 0.8.
     */
    Q_PROPERTY(qreal idealWidthRatio MEMBER m_idealWidthRatio NOTIFY idealWidthRatioChanged)
    /**
     * Left margin size, as a ratio of the short side of layout area. Default is 0.07.
     * The margins are added to each window before layout.
     */
    Q_PROPERTY(qreal relativeMarginLeft MEMBER m_relativeMarginLeft NOTIFY relativeMarginLeftChanged)
    /**
     * Right margin size, as a ratio of the short side of layout area. Default is 0.07.
     * The margins are added to each window before layout.
     */
    Q_PROPERTY(qreal relativeMarginRight MEMBER m_relativeMarginRight NOTIFY relativeMarginRightChanged)
    /**
     * Top margin size, as a ratio of the short side of layout area. Default is 0.07.
     * The margins are added to each window before layout.
     */
    Q_PROPERTY(qreal relativeMarginTop MEMBER m_relativeMarginTop NOTIFY relativeMarginTopChanged)
    /**
     * Bottom margin size, as a ratio of the short side of layout area. Default is 0.07.
     * The margins are added to each window before layout.
     */
    Q_PROPERTY(qreal relativeMarginBottom MEMBER m_relativeMarginBottom NOTIFY relativeMarginBottomChanged)
    /**
     * Minimal length of windows, as a ratio of the short side of layout area.
     * Smaller windows will be resized to this. Default is 0.15.
     */
    Q_PROPERTY(qreal relativeMinLength MEMBER m_relativeMinLength NOTIFY relativeMinLengthChanged)
    /**
     * Maximum additional gap between windows, as a ratio of normal spacing (2*margin). Default is 1.5.
     */
    Q_PROPERTY(qreal maxGapRatio MEMBER m_maxGapRatio NOTIFY maxGapRatioChanged)
    /**
     * Maximum scale applied to windows, *after* the minimum length is enforced. Default is 1.0.
     */
    Q_PROPERTY(qreal maxScale MEMBER m_maxScale NOTIFY maxScaleChanged)

public:
    enum PlacementMode : uint {
        Rows,
        Columns,
    };
    Q_ENUM(PlacementMode)

    explicit ExpoLayout(QQuickItem *parent = nullptr);

    PlacementMode placementMode() const;
    void setPlacementMode(PlacementMode mode);

    void addCell(ExpoCell *cell);
    void removeCell(ExpoCell *cell);

    bool isReady() const;
    void setReady();

    Q_INVOKABLE void forceLayout();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void updatePolish() override;

    /**
     * @brief Layout the windows with @param windowSizes into @param area.
     *
     * This is the main entry point for the layout algorithm.
     */
    QList<QRectF> layout(const QRectF &area, const QList<QRectF> &windowSizes);

    /**
     * @brief First clip @param windowSizes to be between @param minSize and
     * @param maxSize. Then add @param margins to each window size, and @return
     * the adjusted window sizes.
     */
    QList<QRectF> adjustSizes(const QRectF &minSize, const QRectF &maxSize, const QMarginsF &margins, const QList<QRectF> &windowSizes);

    /**
     * @brief Use binary search to find a good packing of the @param windowSizes
     * into @param area such that the resulting packing has similar aspect ratio
     * (height/width) to @param area.
     *
     * The binary search is performed on the logarithm of the width of the
     * possible packings, and the search is terminated when the width of the
     * packing is within @param tol of the ideal width.
     *
     * We try to find a packing such that the total widths of windows in each
     * layer are close to @param idealWidthRatio times the maximum width of the
     * packing.
     *
     * In the case of identical window heights, we also try to minimize vertical
     * movement based on the @param centers of the windows.
     *
     * Run time is O(n log n log log (totalWidth / maxWidth))
     * Since we clip the window size, this is just O(n log n log log n)
     */
    LayeredPacking
    findGoodPacking(const QRectF &area, const QList<QRectF> &windowSizes, const QList<QPointF> &centers, qreal idealWidthRatio, qreal tol);

    /**
     * @brief Output the final window layouts from the packing.
     *
     * Geven @param windowSizes, scale @param packing to fit @param area,
     * remove previously added @param margins, add padding and align,
     * and @return the final layout.
     * In each layer, sort the windows by x coordinates of the @param centers.
     */
    QList<QRectF> refineAndApplyPacking(const QRectF &area, const QMarginsF &margins, const LayeredPacking &packing, const QList<QRectF> &windowSizes, const QList<QPointF> &centers);

Q_SIGNALS:
    void placementModeChanged();
    void readyChanged();
    void searchToleranceChanged();
    void idealWidthRatioChanged();
    void relativeMarginLeftChanged();
    void relativeMarginRightChanged();
    void relativeMarginTopChanged();
    void relativeMarginBottomChanged();
    void relativeMinLengthChanged();
    void maxGapRatioChanged();
    void maxScaleChanged();

private:
    QList<ExpoCell *> m_cells;
    PlacementMode m_placementMode = Rows;
    bool m_ready = false;

    qreal m_searchTolerance = 0.2;
    qreal m_idealWidthRatio = 0.8;
    qreal m_relativeMarginLeft = 0.07;
    qreal m_relativeMarginRight = 0.07;
    qreal m_relativeMarginTop = 0.07;
    qreal m_relativeMarginBottom = 0.07;
    qreal m_relativeMinLength = 0.15;
    qreal m_maxGapRatio = 1.5;
    qreal m_maxScale = 1.0;
};

class ExpoCell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ExpoLayout *layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int naturalX READ naturalX WRITE setNaturalX NOTIFY naturalXChanged)
    Q_PROPERTY(int naturalY READ naturalY WRITE setNaturalY NOTIFY naturalYChanged)
    Q_PROPERTY(int naturalWidth READ naturalWidth WRITE setNaturalWidth NOTIFY naturalWidthChanged)
    Q_PROPERTY(int naturalHeight READ naturalHeight WRITE setNaturalHeight NOTIFY naturalHeightChanged)
    Q_PROPERTY(int x READ x NOTIFY xChanged)
    Q_PROPERTY(int y READ y NOTIFY yChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(QString persistentKey READ persistentKey WRITE setPersistentKey NOTIFY persistentKeyChanged)
    Q_PROPERTY(int bottomMargin READ bottomMargin WRITE setBottomMargin NOTIFY bottomMarginChanged)

public:
    explicit ExpoCell(QObject *parent = nullptr);
    ~ExpoCell() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    ExpoLayout *layout() const;
    void setLayout(ExpoLayout *layout);

    int naturalX() const;
    void setNaturalX(int x);

    int naturalY() const;
    void setNaturalY(int y);

    int naturalWidth() const;
    void setNaturalWidth(int width);

    int naturalHeight() const;
    void setNaturalHeight(int height);

    QRect naturalRect() const;
    QMargins margins() const;

    int x() const;
    void setX(int x);

    int y() const;
    void setY(int y);

    int width() const;
    void setWidth(int width);

    int height() const;
    void setHeight(int height);

    QString persistentKey() const;
    void setPersistentKey(const QString &key);

    int bottomMargin() const;
    void setBottomMargin(int margin);

public Q_SLOTS:
    void update();

Q_SIGNALS:
    void layoutChanged();
    void enabledChanged();
    void naturalXChanged();
    void naturalYChanged();
    void naturalWidthChanged();
    void naturalHeightChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void persistentKeyChanged();
    void bottomMarginChanged();

private:
    QString m_persistentKey;
    bool m_enabled = true;
    int m_naturalX = 0;
    int m_naturalY = 0;
    int m_naturalWidth = 0;
    int m_naturalHeight = 0;
    QMargins m_margins;
    std::optional<int> m_x;
    std::optional<int> m_y;
    std::optional<int> m_width;
    std::optional<int> m_height;
    QPointer<ExpoLayout> m_layout;
};

/**
 * @brief Each Layer is a horizontal strip of windows with a maximum width and
 * height.
 */
struct Layer
{
    qreal maxWidth;
    qreal maxHeight;
    /**
     * @brief The remaining width available to new windows in this layer.
     * width() + remainingWidth() == maxWidth
     */
    qreal remainingWidth;

    /**
     * @brief The indices of windows in this layer.
     */
    QList<size_t> ids;

    /**
     * @brief Initializes a new layer with the given maximum width and populates
     * it with the given windows.
     *
     * @param maxWidth The maximum width of the layer.
     * @param windowSizes The sizes of all the windows. Must be sorted in
     * ascending order by height.
     * @param windowIds Ids of the windows.
     * @param startPos windowIds[startPos] is the first window in this layer.
     * @param endPos windowIds[endPos-1] is the last window in this layer.
     */
    Layer(qreal maxWidth, const QList<QRectF> &windowSizes, const QList<size_t> &windowIds, size_t startPos, size_t endPos);

    /**
     * @brief The total width of all the windows in this layer.
     *
     */
    qreal width() const;
};

/**
 * @brief A LayeredPacking is a packing of windows into layers, which are
 * horizontal strips of windows.
 */
struct LayeredPacking
{
    qreal maxWidth;
    qreal width;
    qreal height;
    QList<Layer> layers;

    /**
     * @brief Construct a new LayeredPacking object from a list of windows
     * sorted by height in descending order.
     *
     * @param maxWidth The maximum width of the packing.
     * @param windowSizes must be sorted by height in ascending order
     * @param ids Ids of the windows
     * @param layerStartPos Array of indices into ids that indicate the start
     * of a new layer. Must start with 0 and end with ids.size().
     */
    LayeredPacking(qreal maxWidth, const QList<QRectF> &windowSizes, const QList<size_t> &ids, const QList<size_t> &layerStartPos);
};
