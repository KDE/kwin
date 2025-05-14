/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"
#include <QObject>

class QAction;

namespace KWin
{

/*!
 * \qmltype ScreenEdgeHandler
 * \inqmlmodule org.kde.kwin
 *
 * \brief Qml export for reserving a Screen Edge.
 *
 * The edge is controlled by the enabled property and the edge
 * property. If the edge is enabled and gets triggered the activated()
 * signal gets emitted.
 *
 * Example usage:
 * \code
 * ScreenEdgeHandler {
 *     edge: ScreenEdgeHandler.LeftEdge
 *     onActivated: doSomething()
 * }
 * \endcode
 */
class ScreenEdgeHandler : public QObject
{
    Q_OBJECT
    /*!
     * \qmlproperty bool ScreenEdgeHandler::enabled
     *
     * Whether the edge is currently enabled, that is reserved. Default value is \c true.
     */
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    /*!
     * \qmlproperty enumeration ScreenEdgeHandler::edge
     * \qmlenumeratorsfrom KWin::ScreenEdgeHandler::Edge
     *
     * Which of the screen edges is to be reserved. Default value is NoEdge.
     */
    Q_PROPERTY(Edge edge READ edge WRITE setEdge NOTIFY edgeChanged)
    /*!
     * \qmlproperty enumeration ScreenEdgeHandler::mode
     * \qmlenumeratorsfrom KWin::ScreenEdgeHandler::Mode
     * The operation mode for this edge. Default value is Mode::Pointer
     */
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
public:
    /*!
     * The Edge type specifies a resesrved screen edge.
     *
     * \value TopEdge The top edge of the screen.
     * \value TopRightEdge The top right corner of the screen.
     * \value RightEdge The right edge of the screen.
     * \value BottomRightEdge The bottom right corner of the screen.
     * \value BottomEdge The bottom edge of the screen.
     * \value BottomLeftEdge The bottom left corner of the screen.
     * \value LeftEdge The left edge of the screen.
     * \value TopLeftEdge The top left corner of the screen.
     * \omitvalue EDGE_COUNT
     * \value NoEdge No edge.
     */
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
    Q_ENUM(Edge)
    /*!
     * Enum describing the operation modes of the edge.
     * \value Pointer The screen edge is triggered by a pointer input device.
     * \value Touch The screen edge is triggered by a touch input device.
     */
    enum class Mode {
        Pointer,
        Touch
    };
    Q_ENUM(Mode)
    explicit ScreenEdgeHandler(QObject *parent = nullptr);
    ~ScreenEdgeHandler() override;
    bool isEnabled() const;
    Edge edge() const;
    Mode mode() const
    {
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

    /*!
     * \qmlsignal ScreenEdgeHandler::activated()
     *
     * This signal is emitted when the screen edge is activated by the user.
     *
     * The way how the screen edge gets activated depends on the mode(). For example, with
     * Mode::Pointer, the screen edge will be activated when the pointer hits the screen edge;
     * with Mode::Touch, the screen edge will be activated when user swipes their fingers from
     * the corresponding screen border towards the center of the screen.
     */
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

inline bool ScreenEdgeHandler::isEnabled() const
{
    return m_enabled;
}

inline ScreenEdgeHandler::Edge ScreenEdgeHandler::edge() const
{
    return m_edge;
}

} // namespace KWin
