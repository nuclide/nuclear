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

#include "wlshell.h"
#include "shell.h"
#include "shellsurface.h"

WlShell::WlShell()
{
    wl_global_create(Shell::instance()->compositor()->wl_display, &wl_shell_interface, 1, this,
                     [](wl_client *client, void *data, uint32_t version, uint32_t id) {
                         static_cast<WlShell *>(data)->bind(client, version, id);
                     });
}

void WlShell::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_shell_interface, version, id);
    if (resource)
        wl_resource_set_implementation(resource, &shell_implementation, this, nullptr);
}

void WlShell::sendConfigure(weston_surface *surface, uint32_t edges, int32_t width, int32_t height)
{
    ShellSurface *shsurf = Shell::getShellSurface(surface);
    wl_shell_surface_send_configure(shsurf->wl_resource(), edges, width, height);
}

ShellSurface *WlShell::getShellSurface(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface_resource)
{
    weston_surface *surface = static_cast<weston_surface *>(wl_resource_get_user_data(surface_resource));

    ShellSurface *shsurf = Shell::getShellSurface(surface);
    if (shsurf) {
        wl_resource_post_error(surface_resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "WlShell::getShellSurface already requested");
        return shsurf;
    }

    shsurf = Shell::instance()->createShellSurface(surface, &shell_client);
    if (!shsurf) {
        wl_resource_post_error(surface_resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "surface->configure already set");
        return nullptr;
    }

    shsurf->init(client, id);
    shsurf->pingTimeoutSignal.connect(this, &WlShell::pingTimeout);

    return shsurf;
}

void WlShell::pingTimeout(ShellSurface *shsurf)
{
    weston_seat *seat;
    wl_list_for_each(seat, &Shell::instance()->compositor()->seat_list, link) {
        if (seat->pointer->focus == shsurf->view())
            surfaceUnresponsive(shsurf, seat);
    }
}

const weston_shell_client WlShell::shell_client = {
    WlShell::sendConfigure
};

const struct wl_shell_interface WlShell::shell_implementation = {
    [](wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface_resource) {
        static_cast<WlShell *>(wl_resource_get_user_data(resource))->getShellSurface(client, resource, id, surface_resource);
    }
};
