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

#ifndef DESKTOPSHELLWORKSPACE_H
#define DESKTOPSHELLWORKSPACE_H

#include "interface.h"

struct wl_client;
struct wl_resource;

class Workspace;

class DesktopShellWorkspace : public Interface
{
public:
    DesktopShellWorkspace();

    void init(wl_client *client);
    wl_resource *resource() const { return m_resource; }
    Workspace *workspace();

    static DesktopShellWorkspace *fromResource(wl_resource *resource);

protected:
    virtual void added() override;

private:
    void activeChanged();
    void removed(wl_client *client, wl_resource *res);

    wl_resource *m_resource;

    static const struct desktop_shell_workspace_interface s_implementation;
};

#endif
