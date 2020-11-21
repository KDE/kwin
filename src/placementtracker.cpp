/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placementtracker.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "main.h"
#include "platform.h"
#include "screens.h"
#include "workspace.h"

namespace KWin
{

PlacementTracker::PlacementTracker(QObject *parent)
    : QObject(parent)
{
    connect(workspace(), &Workspace::clientAdded, this, &PlacementTracker::handleClientAdded);
    connect(workspace(), &Workspace::clientRemoved, this, &PlacementTracker::handleClientRemoved);
}

static QRect moveBetweenRects(const QRect &sourceArea, const QRect &targetArea, const QRect &rect)
{
    Q_ASSERT(sourceArea.isValid() && targetArea.isValid());

    const int xOffset = rect.x() - sourceArea.x();
    const int yOffset = rect.y() - sourceArea.y();

    const qreal xScale = qreal(targetArea.width()) / sourceArea.width();
    const qreal yScale = qreal(targetArea.height()) / sourceArea.height();

    return QRectF(targetArea.x() + xOffset * xScale, targetArea.y() + yOffset * yScale,
                  rect.width() * xScale, rect.height() * yScale).toRect();
}

void PlacementTracker::rearrange()
{
    const Platform *platform = kwinApp()->platform();

    for (auto it = m_placementData.constBegin(); it != m_placementData.constEnd(); ++it) {
        AbstractClient *window = it.key();
        const PlacementData &data = it.value();

        QRect originalArea;
        QRect originalRect;
        QRect targetArea;

        if (const AbstractOutput *output = platform->findOutput(data.user.outputName)) {
            targetArea = output->geometry();
            originalArea = data.user.outputGeometry;
            originalRect = data.user.geometry;
        } else if (const AbstractOutput *output = platform->findOutput(data.last.outputName)) {
            targetArea = output->geometry();
            originalArea = data.last.outputGeometry;
            originalRect = data.last.geometry;
        } else {
            qCDebug(KWIN_CORE) << "Could not find the original output for window" << window;
            continue;
        }

        if (!originalRect.isValid()) {
            qCDebug(KWIN_CORE) << "Not re-arranging" << window << "due to invalid geometry";
            continue;
        }

        window->setFrameGeometry(moveBetweenRects(originalArea, targetArea, originalRect));
    }
}

void PlacementTracker::handleClientAdded(AbstractClient *client)
{
    if (client->isSpecialWindow() || client->isPopupWindow() || !client->isPlaceable()) {
        return;
    }

    auto updateAllStates = [this, client]() {
        update(client, State::Last | State::User);
    };

    connect(client, &AbstractClient::clientFinishUserMovedResized, this, updateAllStates);
    connect(client, &AbstractClient::frameGeometryChanged,
            this, [client, this]() { update(client, State::Last); });
    connect(client, &AbstractClient::fullScreenChanged, this, updateAllStates);
    connect(client, qOverload<AbstractClient *, MaximizeMode>(&AbstractClient::clientMaximizedStateChanged),
            this, updateAllStates);
    connect(client, &AbstractClient::quickTileModeChanged, this, updateAllStates);

    m_placementData.insert(client, PlacementData());
    update(client, State::Last | State::User);
}

void PlacementTracker::handleClientRemoved(AbstractClient *client)
{
    m_placementData.remove(client);
}

void PlacementTracker::update(AbstractClient *client, States states)
{
    auto it = m_placementData.find(client);
    if (it == m_placementData.end()) {
        return;
    }

    if (!states) {
        states = State::Last | State::User;
    }

    if (states & State::Last) {
        const int screenId = screens()->number(client->frameGeometry().center());
        if (screenId == -1) {
            return;
        }

        const AbstractOutput *output = kwinApp()->platform()->findOutput(screenId);
        it->last.outputName = output->name();
        it->last.geometry = client->frameGeometry();
        it->last.outputGeometry = output->geometry();
    }

    if (states & State::User) {
        it->user = it->last;
    }
}

} // namespace KWin
