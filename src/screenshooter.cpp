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

#include <weston/compositor.h>

#include "screenshooter.h"
#include "shell.h"
#include "wayland-screenshooter-server-protocol.h"

Screenshooter::Screenshooter()
{
    wl_global_create(Shell::instance()->compositor()->wl_display, &screenshooter_interface, 1, this,
                     [](wl_client *client, void *data, uint32_t version, uint32_t id) {
                         static_cast<Screenshooter *>(data)->bind(client, version, id);
                     });
}

void Screenshooter::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &screenshooter_interface, version, id);

    if (Shell::instance()->isTrusted(client, "screenshooter")) {
        wl_resource_set_implementation(resource, &s_implementation, this, nullptr);
        return;
    }

    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "permission to bind screenshooter denied");
    wl_resource_destroy(resource);
}

static void done(void *data, weston_screenshooter_outcome outcome)
{
    wl_resource *resource = static_cast<wl_resource *>(data);

    switch (outcome) {
        case WESTON_SCREENSHOOTER_SUCCESS:
            screenshooter_send_done(resource);
            break;
        case WESTON_SCREENSHOOTER_NO_MEMORY:
            wl_resource_post_no_memory(resource);
            break;
        default:
            break;
    }
}

void Screenshooter::shoot(wl_client *client, wl_resource *resource, wl_resource *output_resource, wl_resource *buffer_resource)
{
    weston_output *output = static_cast<weston_output *>(wl_resource_get_user_data(output_resource));
    weston_buffer *buffer = weston_buffer_from_resource(buffer_resource);

    if (!buffer) {
        wl_resource_post_no_memory(resource);
        return;
    }

    weston_screenshooter_shoot(output, buffer, done, resource);
}

const struct screenshooter_interface Screenshooter::s_implementation = {
    wrapInterface(&Screenshooter::shoot)
};
