/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef DECORATIONTHREAD_H
#define DECORATIONTHREAD_H

#include <QThread>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QImage>
#include <QRect>
#include <QDebug>

#include <vector>
#include <atomic>

namespace KWin
{

namespace Decoration
{
    class DecoratedClientImpl;
}


class VulkanDecorationThread : public QThread
{
    Q_OBJECT

public:
    struct Task {
        Decoration::DecoratedClientImpl *client;
        QImage image;
        QRect rect;
    };

    /**
     * Creates a new VulkanDecorationThread.
     */
    explicit VulkanDecorationThread();

    /**
     * Destroys the VulkanDecorationThread.
     */
    ~VulkanDecorationThread() override;

    /**
     * Waits until the thread has finished processing queued tasks.
     */
    void waitForIdle() {
        if (m_count == 0)
            return;

        QMutexLocker lock(&m_mutex);
        if (m_count > 0)
            m_done.wait(&m_mutex);
    }

    /**
     * Instructs the thread to quit.
     */
    void stop() {
        enqueue(std::vector<Task>{{nullptr, QImage(), QRect()}});
    }

    /**
     * Enqueues rendering tasks and wakes the thread.
     */
    void enqueue(std::vector<Task> &&tasks) {
        QMutexLocker lock(&m_mutex);

        const auto count = tasks.size();

        if (m_tasks.empty()) {
            m_tasks = std::move(tasks);
        } else {
            m_tasks.reserve(m_tasks.size() + tasks.size());
            for (auto &&task : tasks)
                m_tasks.emplace_back(std::move(task));
        }

        m_count += count;
        m_newTasks.wakeAll();
    }

protected:
    /**
     * @internal
     */
    void run() override;

private:
    QMutex m_mutex;
    QWaitCondition m_newTasks;
    QWaitCondition m_done;
    std::vector<Task> m_tasks;
    std::atomic<uint32_t> m_count;
    bool m_quit;
};

} // namespace KWin

#endif // DECORATIONTHREAD_H
