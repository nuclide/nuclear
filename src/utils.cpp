/*
 * Copyright 2014 Giulio Camuffo <giuliocamuffo@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "utils.h"
#include "shell.h"

Timer::Timer(int interval)
     : m_interval(interval)
     , m_source(nullptr)
{
}

Timer::~Timer()
{
    stop();
}

void Timer::start()
{
    if (!m_source) {
        wl_event_loop *loop = wl_display_get_event_loop(Shell::compositor()->wl_display);
        m_source = wl_event_loop_add_timer(loop, [](void *data) { static_cast<Timer *>(data)->triggered(); return 1; }, this);
        wl_event_source_timer_update(m_source, m_interval);
    }
}

void Timer::stop()
{
    if (m_source) {
        wl_event_source_remove(m_source);
        m_source = nullptr;
    }
}

bool Timer::isRunning() const
{
    return m_source != nullptr;
}
