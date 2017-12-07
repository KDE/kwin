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

#include "decorationthread.h"
#include "decorations/decoratedclient.h"

#include <KDecoration2/Decoration>

#include <QPainter>
#include <QDebug>


namespace KWin
{


VulkanDecorationThread::VulkanDecorationThread()
{
    m_count = 0;
    m_quit = false;
}


VulkanDecorationThread::~VulkanDecorationThread()
{
}


void VulkanDecorationThread::run()
{
    bool quit = false;

    m_mutex.lock();

    while (!quit) {
        if (m_tasks.empty()) {
            m_done.wakeAll();
            m_newTasks.wait(&m_mutex);
        }

        std::vector<Task> tasks = std::move(m_tasks);
        m_tasks = std::vector<Task>();

        m_mutex.unlock();

        for (Task &task : tasks) {
            if (task.client == nullptr) {
                quit = true;
                break;
            }

            task.image.fill(0);

            QPainter p(&task.image);
            p.setRenderHint(QPainter::Antialiasing);
            p.setWindow(QRect(task.rect.topLeft(), task.rect.size() * task.image.devicePixelRatio()));
            p.setClipRect(task.rect);
            task.client->decoration()->paint(&p, task.rect);
        }

        m_mutex.lock();
        m_count -= tasks.size();
    }

    m_mutex.unlock();
}


} // namespace KWin


