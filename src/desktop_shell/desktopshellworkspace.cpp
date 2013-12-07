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

#include "desktopshellworkspace.h"
#include "workspace.h"

#include "wayland-desktop-shell-server-protocol.h"

DesktopShellWorkspace::DesktopShellWorkspace()
                     : Interface()
{
}

void DesktopShellWorkspace::added()
{
    workspace()->activeChangedSignal.connect(this, &DesktopShellWorkspace::activeChanged);
}

void DesktopShellWorkspace::init(wl_client *client)
{
    m_resource = wl_resource_create(client, &desktop_shell_workspace_interface, 1, 0);
    wl_resource_set_implementation(m_resource, &s_implementation, this, 0);
}

Workspace *DesktopShellWorkspace::workspace()
{
    return static_cast<Workspace *>(object());
}

DesktopShellWorkspace *DesktopShellWorkspace::fromResource(wl_resource *res)
{
    return static_cast<DesktopShellWorkspace *>(wl_resource_get_user_data(res));
}

void DesktopShellWorkspace::activeChanged()
{
    if (workspace()->isActive()) {
        desktop_shell_workspace_send_activated(m_resource);
    } else {
        desktop_shell_workspace_send_deactivated(m_resource);
    }
}

void DesktopShellWorkspace::removed(wl_client *client, wl_resource *res)
{
    object()->destroy();
}

const struct desktop_shell_workspace_interface DesktopShellWorkspace::s_implementation = {
    wrapInterface(&DesktopShellWorkspace::removed)
};
