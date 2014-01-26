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

#include "desktopshellwindow.h"
#include "shell.h"
#include "shellsurface.h"

#include "wayland-desktop-shell-server-protocol.h"

DesktopShellWindow::DesktopShellWindow()
                  : Interface()
                  , m_resource(nullptr)
                  , m_state(DESKTOP_SHELL_WINDOW_STATE_INACTIVE)
{
}

DesktopShellWindow::~DesktopShellWindow()
{
    destroy();
}

void DesktopShellWindow::added()
{
    shsurf()->typeChangedSignal.connect(this, &DesktopShellWindow::surfaceTypeChanged);
    shsurf()->titleChangedSignal.connect(this, &DesktopShellWindow::sendTitle);
    shsurf()->activeChangedSignal.connect(this, &DesktopShellWindow::activeChanged);
    shsurf()->mappedSignal.connect(this, &DesktopShellWindow::mapped);
    shsurf()->unmappedSignal.connect(this, &DesktopShellWindow::destroy);
}

ShellSurface *DesktopShellWindow::shsurf()
{
    return static_cast<ShellSurface *>(object());
}

void DesktopShellWindow::mapped()
{
    if (m_resource) {
        return;
    }

    ShellSurface::Type type = shsurf()->type();
    if (type == ShellSurface::Type::TopLevel || type == ShellSurface::Type::Maximized || type == ShellSurface::Type::Fullscreen) {
        create();
    }
}

void DesktopShellWindow::surfaceTypeChanged()
{
    ShellSurface::Type type = shsurf()->type();
    if (type == ShellSurface::Type::TopLevel || type == ShellSurface::Type::Maximized || type == ShellSurface::Type::Fullscreen) {
        if (!m_resource) {
            create();
        }
    } else {
        destroy();
    }
}

void DesktopShellWindow::activeChanged()
{
    if (shsurf()->isActive()) {
        m_state |= DESKTOP_SHELL_WINDOW_STATE_ACTIVE;
    } else {
        m_state &= ~DESKTOP_SHELL_WINDOW_STATE_ACTIVE;
    }
    sendState();
}

void DesktopShellWindow::create()
{
    m_resource = wl_resource_create(Shell::instance()->shellClient(), &desktop_shell_window_interface, 1, 0);
    wl_resource_set_implementation(m_resource, &s_implementation, this, 0);
    desktop_shell_send_window_added(Shell::instance()->shellClientResource(), m_resource, shsurf()->title().c_str(), m_state);
}

void DesktopShellWindow::destroy()
{
    if (m_resource) {
        desktop_shell_window_send_removed(m_resource);
        wl_resource_destroy(m_resource);
        m_resource = nullptr;
    }
}

void DesktopShellWindow::sendState()
{
    if (m_resource) {
        desktop_shell_window_send_state_changed(m_resource, m_state);
    }
}

void DesktopShellWindow::sendTitle()
{
    if (m_resource) {
        desktop_shell_window_send_set_title(m_resource, shsurf()->title().c_str());
    }
}

void DesktopShellWindow::setState(wl_client *client, wl_resource *resource, int32_t state)
{
    ShellSurface *s = shsurf();

    if (m_state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED && !(state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        s->setMinimized(false);
    } else if (state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED && !(m_state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        s->setMinimized(true);
        if (s->isActive()) {
            s->deactivate();
        }
    }

    if (state & DESKTOP_SHELL_WINDOW_STATE_ACTIVE && !(state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        s->activate();
    }

    m_state = state;
    sendState();
}

void DesktopShellWindow::close(wl_client *client, wl_resource *resource)
{
    shsurf()->close();
}

const struct desktop_shell_window_interface DesktopShellWindow::s_implementation = {
    wrapInterface(&DesktopShellWindow::setState),
    wrapInterface(&DesktopShellWindow::close)
};
