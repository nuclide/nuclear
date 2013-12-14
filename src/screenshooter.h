/*
 * Copyright 2013-2014 Giulio Camuffo <giuliocamuffo@gmail.com>
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

#ifndef SCREENSHOOTER_H
#define SCREENSHOOTER_H

#include <wayland-server.h>

#include "interface.h"

class Screenshooter : public Interface
{
public:
    Screenshooter();

private:
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void shoot(wl_client *client, wl_resource *resource, wl_resource *output_resource, wl_resource *buffer_resource);

    static const struct screenshooter_interface s_implementation;
};

#endif
