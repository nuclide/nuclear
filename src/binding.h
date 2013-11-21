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

#ifndef BINDING_H
#define BINDING_H

#include <weston/compositor.h>

#include "shellsignal.h"

struct weston_seat;

class Binding {
public:
    enum class Type {
        Key = 0,
        Axis = 1
    };
    explicit Binding(Type t);
    ~Binding();

    void bindKey(uint32_t key, weston_keyboard_modifier modifier);
    void bindAxis(uint32_t axis, weston_keyboard_modifier modifier);

    Signal<weston_seat *, uint32_t, uint32_t> keyTriggered;
    Signal<weston_seat *, uint32_t, uint32_t, wl_fixed_t> axisTriggered;

private:
    weston_binding *m_binding;
};

#endif
