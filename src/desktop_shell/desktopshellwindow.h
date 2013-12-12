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

#ifndef DESKTOPSHELLWINDOW_H
#define DESKTOPSHELLWINDOW_H

#include <wayland-server.h>

#include "interface.h"

class ShellSurface;

class DesktopShellWindow : public Interface
{
public:
    DesktopShellWindow();
    ~DesktopShellWindow();

    void create();

protected:
    virtual void added() override;

private:
    ShellSurface *shsurf();
    void surfaceTypeChanged();
    void activeChanged();
    void destroy();
    void sendState();
    void sendTitle();
    void setState(wl_client *client, wl_resource *resource, int32_t state);
    void close(wl_client *client, wl_resource *resource);

    wl_resource *m_resource;
    int32_t m_state;

    static const struct desktop_shell_window_interface s_implementation;
};

#endif
