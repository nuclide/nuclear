/*
 * Copyright 2013  Giulio Camuffo <giuliocamuffo@gmail.com>
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

#include "binding.h"
#include "shell.h"

Binding *Binding::s_toggledBinding = nullptr;

Binding::Binding(Type t)
        : m_binding(nullptr)
        , m_isHotSpot(false)
        , m_isToggle(false)
{
}

Binding::~Binding()
{
    weston_binding_destroy(m_binding);
    if (m_isHotSpot) {
        Shell::instance()->removeHotSpotBinding(this);
    }
}

void Binding::setIsToggle(bool t)
{
    m_isToggle = t;
}

void Binding::releaseToggle()
{
    if (m_isToggle && s_toggledBinding == this) {
        s_toggledBinding = nullptr;
    }
}

void Binding::keyHandler(weston_seat *seat, uint32_t time, uint32_t key, void *data)
{
    Binding *b = static_cast<Binding *>(data);
    if (b->checkToggled()) {
        b->keyTriggered(seat, time, key);
    }
}

static void axisHandler(weston_seat *seat, uint32_t time, uint32_t axis, wl_fixed_t value, void *data)
{
    static_cast<Binding *>(data)->axisTriggered(seat, time, axis, value);
}

void Binding::bindKey(uint32_t key, weston_keyboard_modifier modifier)
{
    m_binding = weston_compositor_add_key_binding(Shell::instance()->compositor(), key, modifier, keyHandler, this);
}

void Binding::bindAxis(uint32_t axis, weston_keyboard_modifier modifier)
{
    m_binding = weston_compositor_add_axis_binding(Shell::instance()->compositor(), axis, modifier, axisHandler, this);
}

void Binding::bindHotSpot(HotSpot hs)
{
    m_isHotSpot = true;
    Shell::instance()->bindHotSpot(hs, this);
}

void Binding::hotSpotHandler(weston_seat *seat, uint32_t time, HotSpot hs)
{
    if (checkToggled()) {
        hotSpotTriggered(seat, time, hs);
    }
}

bool Binding::checkToggled()
{
    if (!m_isToggle) {
        return true;
    }

    if (s_toggledBinding && s_toggledBinding != this) {
        return false;
    }
    if (s_toggledBinding == this) {
        s_toggledBinding = nullptr;
    } else {
        s_toggledBinding = this;
    }
    return true;
}
