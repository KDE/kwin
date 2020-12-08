/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENEDGEITEM_H
#define KWIN_SCREENEDGEITEM_H

#include <QObject>
#include <kwinglobals.h>

class QAction;

namespace KWin
{

/**
 * @brief Qml export for reserving a Screen Edge.
 *
 * The edge is controlled by the @c enabled property and the @c edge
 * property. If the edge is enabled and gets triggered the @ref activated()
 * signal gets emitted.
 *
 * Example usage:
 * @code
 * ScreenEdgeItem {
 *     edge: ScreenEdgeItem.LeftEdge
 *     onActivated: doSomething()
 * }
 * @endcode
 */
class ScreenEdgeItem : public QObject
{
    Q_OBJECT
    Q_ENUMS(Edge)
    Q_ENUMS(Mode)
    /**
     * @brief Whether the edge is currently enabled, that is reserved. Default value is @c true.
     */
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    /**
     * @brief Which of the screen edges is to be reserved. Default value is @c NoEdge.
     */
    Q_PROPERTY(Edge edge READ edge WRITE setEdge NOTIFY edgeChanged)
    /**
     * @brief The operation mode for this edge. Default value is @c Mode::Pointer
     */
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
public:
    enum Edge {
        TopEdge,
        TopRightEdge,
        RightEdge,
        BottomRightEdge,
        BottomEdge,
        BottomLeftEdge,
        LeftEdge,
        TopLeftEdge,
        EDGE_COUNT,
        NoEdge
    };
    /**
     * Enum describing the operation modes of the edge.
     */
    enum class Mode {
        Pointer,
        Touch
    };
    explicit ScreenEdgeItem(QObject *parent = nullptr);
    ~ScreenEdgeItem() override;
    bool isEnabled() const;
    Edge edge() const;
    Mode mode() const {
        return m_mode;
    }

public Q_SLOTS:
    void setEnabled(bool enabled);
    void setEdge(Edge edge);
    void setMode(Mode mode);

Q_SIGNALS:
    void enabledChanged();
    void edgeChanged();
    void modeChanged();

    void activated();

private Q_SLOTS:
    bool borderActivated(ElectricBorder edge);
private:
    void enableEdge();
    void disableEdge();
    bool m_enabled;
    Edge m_edge;
    Mode m_mode = Mode::Pointer;
    QAction *m_action;
};

inline bool ScreenEdgeItem::isEnabled() const
{
    return m_enabled;
}

inline ScreenEdgeItem::Edge ScreenEdgeItem::edge() const
{
    return m_edge;
}

} // namespace KWin

#endif //  KWIN_SCREENEDGEITEM_H
