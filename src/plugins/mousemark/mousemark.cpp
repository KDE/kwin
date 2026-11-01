/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2023 Andrew Shark <ashark at linuxcomp.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mousemark.h"
#include "mousemarklogging.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/workspacescene.h"
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QPainter>

#include <cmath>

namespace KWin
{

/**
 * The lines of one mark, with a width in device pixels. Short segments are painted into square
 * tiles, one image per tile. A segment longer than a quarter of a tile is a one pixel image of the
 * mark color, stretched to the length of the segment and turned along it.
 */
class MarkItem : public Item
{
public:
    MarkItem(const QColor &color, int width, Item *parent)
        : Item(parent)
        , m_color(color)
        , m_width(width)
        , m_pixel(1, 1, QImage::Format_ARGB32_Premultiplied)
    {
        m_pixel.fill(color);
    }

    void addSegment(const QPointF &from, const QPointF &to)
    {
        const qreal scale = scaleAt(from);
        const qreal width = m_width / scale;
        const QLineF line(from, to);
        // A new tile is centered on the segment, so a short segment and its line width fit in it
        const qreal margin = width / 2 + 1;
        if (line.length() > TileSize / 4 || margin > TileSize / 4) {
            addLine(line, width);
            return;
        }

        if (m_tiles.empty() || m_tiles.back().scale != scale
            || !m_tiles.back().rect.adjusted(margin, margin, -margin, -margin).contains(from)
            || !m_tiles.back().rect.adjusted(margin, margin, -margin, -margin).contains(to)) {
            addTile(line.center(), scale);
        }
        Tile &tile = m_tiles.back();
        QPainter painter(&tile.image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(line.translated(-tile.rect.topLeft()));
        painter.end();
        tile.item->setImage(tile.image);
    }

    void setMark(const QList<QPointF> &mark)
    {
        // An arrow keeps its number of segments, so its stretched lines are moved where they are
        const qsizetype count = std::max<qsizetype>(mark.size() - 1, 0);
        if (m_tiles.empty() && qsizetype(m_lines.size()) == count) {
            bool allLong = true;
            for (qsizetype i = 0; i < count && allLong; ++i) {
                allLong = QLineF(mark[i], mark[i + 1]).length() > TileSize / 4;
            }
            if (allLong) {
                for (qsizetype i = 0; i < count; ++i) {
                    const QLineF line(mark[i], mark[i + 1]);
                    setLineGeometry(m_lines[i].get(), line, lineWidthAt(line.p1()));
                }
                return;
            }
        }

        m_tiles.clear();
        m_lines.clear();
        for (qsizetype i = 0; i < count; ++i) {
            addSegment(mark[i], mark[i + 1]);
        }
    }

private:
    static constexpr qreal TileSize = 128;

    struct Tile
    {
        RectF rect;
        qreal scale;
        QImage image;
        std::unique_ptr<ImageItem> item;
    };

    void addTile(const QPointF &center, qreal scale)
    {
        Tile tile{
            .rect = RectF(center.x() - TileSize / 2, center.y() - TileSize / 2, TileSize, TileSize),
            .scale = scale,
            .image = QImage(QSize(TileSize, TileSize) * scale, QImage::Format_ARGB32_Premultiplied),
            .item = std::make_unique<ImageItem>(this),
        };
        tile.image.setDevicePixelRatio(scale);
        tile.image.fill(Qt::transparent);
        tile.item->setPosition(tile.rect.topLeft());
        tile.item->setSize(tile.rect.size());
        m_tiles.push_back(std::move(tile));
    }

    static qreal scaleAt(const QPointF &point)
    {
        LogicalOutput *output = effects->screenAt(point.toPoint());
        return output ? output->scale() : 1.0;
    }

    qreal lineWidthAt(const QPointF &point) const
    {
        return m_width / scaleAt(point);
    }

    void addLine(const QLineF &line, qreal width)
    {
        auto item = std::make_unique<ImageItem>(this);
        item->setImage(m_pixel);
        setLineGeometry(item.get(), line, width);
        m_lines.push_back(std::move(item));
    }

    static void setLineGeometry(ImageItem *item, const QLineF &line, qreal width)
    {
        item->setPosition(line.p1());
        item->setSize(QSizeF(line.length(), width));
        item->setTransform(QTransform().rotate(-line.angle()).translate(0, -width / 2));
    }

    QColor m_color;
    int m_width;
    QImage m_pixel;
    std::vector<Tile> m_tiles;
    std::vector<std::unique_ptr<ImageItem>> m_lines;
};

MouseMarkEffect::MouseMarkEffect()
{
    MouseMarkConfig::instance(effects->config());
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear All Mouse Marks"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_F11));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clear);
    a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_F12));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clearLast);

    rootItem = std::make_unique<Item>(effects->scene()->overlayItem());
    rootItem->setVisible(!effects->isScreenLocked());

    connect(effects, &EffectsHandler::mouseChanged, this, &MouseMarkEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &MouseMarkEffect::screenLockingChanged);
    reconfigure(ReconfigureAll);
}

MouseMarkEffect::~MouseMarkEffect()
{
}

static int width_2 = 1;
void MouseMarkEffect::reconfigure(ReconfigureFlags)
{
    m_freedraw_modifiers = Qt::KeyboardModifiers();
    m_arrowdraw_modifiers = Qt::KeyboardModifiers();
    MouseMarkConfig::self()->read();
    width = MouseMarkConfig::lineWidth();
    width_2 = width / 2;
    color = MouseMarkConfig::color();
    color.setAlphaF(1.0);
    if (MouseMarkConfig::freedrawshift()) {
        m_freedraw_modifiers |= Qt::ShiftModifier;
    }
    if (MouseMarkConfig::freedrawalt()) {
        m_freedraw_modifiers |= Qt::AltModifier;
    }
    if (MouseMarkConfig::freedrawcontrol()) {
        m_freedraw_modifiers |= Qt::ControlModifier;
    }
    if (MouseMarkConfig::freedrawmeta()) {
        m_freedraw_modifiers |= Qt::MetaModifier;
    }

    if (MouseMarkConfig::arrowdrawshift()) {
        m_arrowdraw_modifiers |= Qt::ShiftModifier;
    }
    if (MouseMarkConfig::arrowdrawalt()) {
        m_arrowdraw_modifiers |= Qt::AltModifier;
    }
    if (MouseMarkConfig::arrowdrawcontrol()) {
        m_arrowdraw_modifiers |= Qt::ControlModifier;
    }
    if (MouseMarkConfig::arrowdrawmeta()) {
        m_arrowdraw_modifiers |= Qt::MetaModifier;
    }

    // Show the marks on screen with the new color and width
    for (qsizetype i = 0; i < marks.size(); ++i) {
        markItems[i] = createMarkItem(marks[i]);
    }
    for (auto it = drawings.cbegin(); it != drawings.cend(); ++it) {
        drawingItems[it.key()] = createMarkItem(it.value());
    }
}

void MouseMarkEffect::setState(State newState)
{
    if (state == newState) {
        return;
    }

    state = newState;
    switch (state) {
    case State::NONE:
        // flush all
        endDrawings();
        break;
    case State::ARROW:
    case State::FREEHAND:
        // Flush drawings, and continue new marks from there
        for (auto it = drawings.begin(); it != drawings.end(); ++it) {
            Mark &mark = it.value();
            if (mark.size() >= 2) {
                marks.append(mark);
                markItems.push_back(std::move(drawingItems[it.key()]));
                mark = {mark.last()};
                drawingItems[it.key()] = createMarkItem(mark);
            }
        }
        break;
    }
}

void MouseMarkEffect::processPoint(qint32 id, const QPointF &pos)
{
    if (state == State::NONE) {
        return;
    }
    Mark &drawing = drawings[id];
    std::unique_ptr<MarkItem> &drawingItem = drawingItems[id];
    if (!drawingItem) {
        drawingItem = createMarkItem(drawing);
    }
    switch (state) {
    case State::NONE:
        return;
    case State::FREEHAND: {
        if (drawing.isEmpty()) {
            drawing.append(pos);
        }
        if (drawing.last() == pos) {
            return;
        }
        drawingItem->addSegment(drawing.last(), pos);
        drawing.append(pos);
        break;
    }
    case State::ARROW: {
        if (drawing.size() < 1) {
            // New arrow
            drawing.append(pos);
        } else if (drawing.back() != pos) {
            // update existing arrow
            QPointF tail = drawing.front();
            drawing = createArrow(pos, tail);
            drawingItem->setMark(drawing);
        }
        break;
    }
    }
}

void MouseMarkEffect::endDraw(qint32 channel)
{
    const auto it = drawings.find(channel);
    if (it == drawings.end()) {
        return;
    }

    if (it->size() >= 2) {
        marks.append(std::move(*it));
        markItems.push_back(std::move(drawingItems[channel]));
    }

    drawings.erase(it);
    drawingItems.erase(channel);
}

void MouseMarkEffect::endDrawings()
{
    for (auto it = drawings.begin(); it != drawings.end(); ++it) {
        if (it->size() >= 2) {
            marks.append(std::move(it.value()));
            markItems.push_back(std::move(drawingItems[it.key()]));
        }
    }
    drawings.clear();
    drawingItems.clear();
}

std::unique_ptr<MarkItem> MouseMarkEffect::createMarkItem(const Mark &mark) const
{
    auto item = std::make_unique<MarkItem>(color, width, rootItem.get());
    for (qsizetype i = 1; i < mark.size(); ++i) {
        item->addSegment(mark[i - 1], mark[i]);
    }
    return item;
}

bool MouseMarkEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchDown id=" << id << " pos=" << pos;
    if (state == State::NONE) {
        return false;
    }
    if (touchPoints.contains(id)) {
        // Will this happen?
        qCWarning(KWIN_MOUSEMARK) << "WARNING: Touch started twice! " << __FILE__ << __LINE__;
        return true;
    }
    touchPoints.insert(id);
    processPoint(id + 1, pos);
    return true;
}

bool MouseMarkEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchMotion id=" << id << " pos=" << pos;
    if (state == State::NONE) {
        if (touchPoints.contains(id)) {
            return true;
        }
        return false;
    }
    processPoint(id + 1, pos);
    return true;
}

bool MouseMarkEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchUp id=" << id;
    if (state == State::NONE) {
        if (touchPoints.contains(id)) {
            touchPoints.remove(id);
            return true;
        }
        return false;
    }
    endDraw(id + 1);
    if (!touchPoints.contains(id)) {
        // touch began before drawing activation
        return false;
    }
    touchPoints.remove(id);
    return true;
}

void MouseMarkEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                       Qt::MouseButtons, Qt::MouseButtons,
                                       Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (effects->isScreenLocked()) {
        return;
    }
    qCDebug(KWIN_MOUSEMARK) << "MouseChanged pos=" << pos;

    if (modifiers == m_freedraw_modifiers && modifiers != Qt::NoModifier) {
        setState(State::FREEHAND);
    } else if (modifiers == m_arrowdraw_modifiers && modifiers != Qt::NoModifier) {
        setState(State::ARROW);
    } else {
        setState(State::NONE);
    }
    processPoint(0, pos);
}

void MouseMarkEffect::clear()
{
    drawings.clear();
    drawingItems.clear();
    marks.clear();
    markItems.clear();
}

void MouseMarkEffect::clearLast()
{
    if (drawings.size() >= 1) { // clear anything currently being drawn first
        drawings.clear();
        drawingItems.clear();
    } else if (!marks.isEmpty()) {
        marks.pop_back();
        markItems.pop_back();
    }
}

MouseMarkEffect::Mark MouseMarkEffect::createArrow(QPointF arrow_head, QPointF arrow_tail)
{
    Mark ret;
    double angle = atan2((double)(arrow_tail.y() - arrow_head.y()), (double)(arrow_tail.x() - arrow_head.x()));
    // Arrow is made of connected lines. Make first one tail, so updates preserve the tail.
    // Last one is head, so freedraw can continue from it
    ret += arrow_tail;
    ret += arrow_head;
    ret += arrow_head + QPoint(50 * cos(angle + M_PI / 6),
                               50 * sin(angle + M_PI / 6)); // right one
    ret += arrow_head;
    ret += arrow_head + QPoint(50 * cos(angle - M_PI / 6),
                               50 * sin(angle - M_PI / 6)); // left one
    ret += arrow_head;
    return ret;
}

void MouseMarkEffect::screenLockingChanged(bool locked)
{
    rootItem->setVisible(!locked);
}

bool MouseMarkEffect::isActive() const
{
    if (effects->isScreenLocked()) {
        return false;
    }
    if (!marks.isEmpty()) {
        return true;
    }

    // only active when stuff is actually drawn
    for (const Mark &mark : drawings) {
        if (mark.size() >= 2) {
            return true;
        }
    }
    return false;
}

int MouseMarkEffect::requestedEffectChainPosition() const
{
    return 10;
}

} // namespace

#include "moc_mousemark.cpp"
