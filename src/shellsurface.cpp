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

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <weston/compositor.h>

#include "shellsurface.h"
#include "shell.h"
#include "shellseat.h"
#include "workspace.h"

#include "wayland-desktop-shell-server-protocol.h"

ShellSurface::ShellSurface(Shell *shell, struct weston_surface *surface)
            : m_shell(shell)
            , m_workspace(nullptr)
            , m_windowResource(nullptr)
            , m_surface(surface)
            , m_view(weston_view_create(surface))
            , m_type(Type::None)
            , m_pendingType(Type::None)
            , m_state(DESKTOP_SHELL_WINDOW_STATE_INACTIVE)
            , m_windowAdvertized(false)
            , m_acceptState(true)
            , m_runningGrab(nullptr)
            , m_parent(nullptr)
{
    m_popup.seat = nullptr;
    wl_list_init(&m_fullscreen.transform.link);
    m_fullscreen.blackView = nullptr;

    m_surfaceDestroyListener.listen(&surface->destroy_signal);
    m_surfaceDestroyListener.signal->connect(static_cast<Object *>(this), &Object::destroy);
}

ShellSurface::~ShellSurface()
{
    if (m_runningGrab) {
        delete m_runningGrab;
    }
    if (m_popup.seat) {
        m_popup.seat->removePopupGrab(this);
    }

    m_shell->removeShellSurface(this);
    if (m_fullscreen.blackView) {
        weston_surface_destroy(m_fullscreen.blackView->surface);
    }
    m_surface->configure = nullptr;
    destroyWindow();
    destroyedSignal();
}

#define _this static_cast<ShellSurface *>(wl_resource_get_user_data(resource))
void ShellSurface::set_state(struct wl_client *client, struct wl_resource *resource, int32_t state)
{
    ShellSurface *shsurf = _this;
    shsurf->setState(state);
}

void ShellSurface::close()
{
    wl_signal_emit(&m_shell->compositor()->kill_signal, m_surface);

    wl_client *client = wl_resource_get_client(m_surface->resource);
    pid_t pid;
    wl_client_get_credentials(client, &pid, NULL, NULL);

    if (pid != getpid()) {
        kill(pid, SIGTERM);
    }
}

const struct desktop_shell_window_interface ShellSurface::m_window_implementation = {
    set_state,
    wrapInterface(&ShellSurface::close)
};

void ShellSurface::destroyWindow()
{
    if (m_windowResource) {
        desktop_shell_window_send_removed(m_windowResource);
        wl_resource_destroy(m_windowResource);
        m_windowResource = nullptr;
    }
}

void ShellSurface::setState(int state)
{
    if (!m_acceptState) {
        return;
    }

    if (m_state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED && !(state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        unminimize();
    } else if (state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED && !(m_state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        minimize();
        if (isActive()) {
            deactivate();
        }
    }

    if (state & DESKTOP_SHELL_WINDOW_STATE_ACTIVE && !(state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED)) {
        activate();
    }

    m_state = state;
    sendState();
}

void ShellSurface::setActive(bool active)
{
    if (active) {
        m_state |= DESKTOP_SHELL_WINDOW_STATE_ACTIVE;
    } else {
        m_state &= ~DESKTOP_SHELL_WINDOW_STATE_ACTIVE;
    }
    sendState();
}

bool ShellSurface::isActive() const
{
    return m_state & DESKTOP_SHELL_WINDOW_STATE_ACTIVE;
}

bool ShellSurface::isMinimized() const
{
    return m_state & DESKTOP_SHELL_WINDOW_STATE_MINIMIZED;
}

void ShellSurface::activate()
{
    weston_seat *seat = container_of(weston_surface()->compositor->seat_list.next, weston_seat, link);
    m_shell->selectWorkspace(m_workspace->number());
    ShellSeat::shellSeat(seat)->activate(this);
}

void ShellSurface::deactivate()
{
    weston_seat *seat = container_of(weston_surface()->compositor->seat_list.next, weston_seat, link);
    ShellSeat::shellSeat(seat)->activate((ShellSurface*)nullptr);
}

void ShellSurface::minimize()
{
    hide();
    minimizedSignal(this);
}

void ShellSurface::unminimize()
{
    show();
    unminimizedSignal(this);
}

void ShellSurface::show()
{
    m_workspace->addSurface(this);
}

void ShellSurface::hide()
{
    wl_list_remove(&m_view->layer_link);
    wl_list_init(&m_view->layer_link);
}

void ShellSurface::sendState()
{
    if (m_windowResource) {
        desktop_shell_window_send_state_changed(m_windowResource, m_state);
    }
}

void ShellSurface::advertize()
{
    m_windowResource = wl_resource_create(m_shell->shellClient(), &desktop_shell_window_interface, 1, 0);
    wl_resource_set_implementation(m_windowResource, &m_window_implementation, this, 0);
    desktop_shell_send_window_added(m_shell->shellClientResource(), m_windowResource, m_title.c_str(), m_state);
    m_windowAdvertized = true;
}

bool ShellSurface::updateType()
{
    if (m_type != m_pendingType && m_pendingType != Type::None) {
        switch (m_type) {
            case Type::Maximized:
                unsetMaximized();
                break;
            case Type::Fullscreen:
                unsetFullscreen();
                break;
            default:
                break;
        }
        m_type = m_pendingType;
        m_pendingType = Type::None;

        switch (m_type) {
            case Type::Maximized:
            case Type::Fullscreen:
                m_savedX = x();
                m_savedY = y();
                break;
            case Type::Transient: {
                weston_view *pv = Shell::defaultView(m_parent);
                weston_view_set_position(m_view, pv->geometry.x + m_transient.x, pv->geometry.y + m_transient.y);
            } break;
            case Type::XWayland:
                weston_view_set_position(m_view, m_transient.x, m_transient.y);
            default:
                break;
        }

        if (m_type == Type::TopLevel || m_type == Type::Maximized || m_type == Type::Fullscreen) {
            if (!m_windowAdvertized) {
                advertize();
            }
        } else {
            destroyWindow();
        }
        return true;
    }
    return false;
}

void ShellSurface::map(int32_t x, int32_t y)
{
    weston_view_geometry_dirty(m_view);

    switch (m_type) {
        case Type::Popup:
            mapPopup();
            break;
        case Type::Fullscreen:
            centerOnOutput(m_fullscreen.output);
            break;
        case Type::Maximized: {
            IRect2D rect = m_shell->windowsArea(m_output);
            x = rect.x;
            y = rect.y;
        }
        case Type::TopLevel:
        case Type::None:
            weston_view_set_position(m_view, x, y);
        default:
            break;
    }

    if (m_type != Type::None) {
        weston_view_update_transform(m_view);
        if (m_type == Type::Maximized) {
            m_view->output = m_output;
        }
    }
}

void ShellSurface::unmapped()
{
    if (m_popup.seat) {
        m_popup.seat->removePopupGrab(this);
        m_popup.seat = nullptr;
    }
}

void ShellSurface::setTopLevel()
{
    m_pendingType = Type::TopLevel;
}

void ShellSurface::setTransient(struct weston_surface *parent, int x, int y, bool inactive)
{
    m_parent = parent;
    m_transient.x = x;
    m_transient.y = y;
    m_transient.inactive = inactive;

    m_pendingType = Type::Transient;
}

void ShellSurface::setXWayland(int x, int y, bool inactive)
{
    // reuse the transient fields for XWayland
    m_transient.x = x;
    m_transient.y = y;
    m_transient.inactive = inactive;

    m_pendingType = Type::XWayland;
}

void ShellSurface::setTitle(const char *title)
{
    m_title = title;
    if (m_windowResource) {
        desktop_shell_window_send_set_title(m_windowResource, title);
    }
}

void ShellSurface::setClass(const char *c)
{
    m_class = c;
}

void ShellSurface::mapPopup()
{
    m_view->output = m_parent->output;

    weston_view_set_transform_parent(m_view, Shell::defaultView(m_parent));
    weston_view_set_position(m_view, m_popup.x, m_popup.y);
    weston_view_update_transform(m_view);

    if (!m_popup.seat->addPopupGrab(this, m_popup.serial)) {
        popupDone();
    }
}

void ShellSurface::addTransform(struct weston_transform *transform)
{
    removeTransform(transform);
    wl_list_insert(&m_view->geometry.transformation_list, &transform->link);

    damage();
}

void ShellSurface::removeTransform(struct weston_transform *transform)
{
    if (wl_list_empty(&transform->link)) {
        return;
    }

    wl_list_remove(&transform->link);
    wl_list_init(&transform->link);

    damage();
}

void ShellSurface::damage()
{
    weston_view_geometry_dirty(m_view);
    weston_view_update_transform(m_view);
    weston_surface_damage(m_surface);
}

void ShellSurface::setAlpha(float alpha)
{
    m_view->alpha = alpha;
    damage();
}

void ShellSurface::popupDone()
{
    popupDoneSignal();
    m_popup.seat = nullptr;
}

bool ShellSurface::isMapped() const
{
    return weston_surface_is_mapped(m_surface);
}

int32_t ShellSurface::x() const
{
    return m_view->geometry.x;
}

int32_t ShellSurface::y() const
{
    return m_view->geometry.y;
}

int32_t ShellSurface::width() const
{
    return m_surface->width;
}

int32_t ShellSurface::height() const
{
    return m_surface->height;
}

float ShellSurface::transformedX() const
{
    pixman_box32_t *box = pixman_region32_extents(&m_view->transform.boundingbox);
    return box->x1;
}

float ShellSurface::transformedY() const
{
    pixman_box32_t *box = pixman_region32_extents(&m_view->transform.boundingbox);
    return box->y1;
}

int32_t ShellSurface::transformedWidth() const
{
    pixman_box32_t *box = pixman_region32_extents(&m_view->transform.boundingbox);
    return box->x2 - box->x1;
}

int32_t ShellSurface::transformedHeight() const
{
    pixman_box32_t *box = pixman_region32_extents(&m_view->transform.boundingbox);
    return box->y2 - box->y1;
}

void ShellSurface::setPosition(float x, float y)
{
    weston_view_set_position(m_view, x, y);
}

float ShellSurface::alpha() const
{
    return m_view->alpha;
}

bool ShellSurface::isPopup() const
{
    return m_popup.seat != nullptr;
}

ShellSurface *ShellSurface::topLevelParent()
{
    if (isPopup() && m_parent) {
        ShellSurface *p = Shell::getShellSurface(m_parent);
        if (p) {
            return p->topLevelParent();
        }
        return nullptr;
    }

    return this;
}

weston_view *ShellSurface::transformParent() const
{
    return m_view->geometry.parent;
}

void ShellSurface::setFullscreen(ShellSurface::FullscreenMethod method, uint32_t framerate, struct weston_output *output)
{
    if (output) {
        m_output = output;
    } else if (m_surface->output) {
        m_output = m_surface->output;
    } else {
        m_output = m_shell->getDefaultOutput();
    }

    m_fullscreen.output = m_output;
    m_fullscreen.type = method;
    m_fullscreen.framerate = framerate;
    m_pendingType = Type::Fullscreen;

    m_client->send_configure(m_surface, 0, m_output->width, m_output->height);
}

void ShellSurface::unsetFullscreen()
{
    m_fullscreen.type = ShellSurface::FullscreenMethod::Default;
    m_fullscreen.framerate = 0;
    removeTransform(&m_fullscreen.transform);
    if (m_fullscreen.blackView) {
        weston_surface_destroy(m_fullscreen.blackView->surface);
    }
    m_fullscreen.blackView = nullptr;
    m_fullscreen.output = nullptr;
    weston_view_set_position(m_view, m_savedX, m_savedY);
}

void ShellSurface::unsetMaximized()
{
    weston_view_set_position(m_view, m_savedX, m_savedY);
}

void ShellSurface::centerOnOutput(struct weston_output *output)
{
    float x = output->x + (output->width - width()) / 2;
    float y = output->y + (output->height - height()) / 2;

    weston_view_set_position(m_view, x, y);
}


// -- Move --

class MoveGrab : public ShellGrab
{
public:
    void motion(uint32_t time, wl_fixed_t x, wl_fixed_t y) override
    {
        weston_pointer_move(pointer(), x, y);

        int dx = wl_fixed_to_int(pointer()->x + this->dx);
        int dy = wl_fixed_to_int(pointer()->y + this->dy);

        if (!shsurf)
            return;

        weston_view *view = shsurf->view();
        weston_view_set_position(view, dx, dy);
        weston_compositor_schedule_repaint(shsurf->m_surface->compositor);
    }
    void button(uint32_t time, uint32_t button, uint32_t state_w) override
    {
        enum wl_pointer_button_state state = (wl_pointer_button_state)state_w;

        if (pointer()->button_count == 0 && state == WL_POINTER_BUTTON_STATE_RELEASED) {
            shsurf->moveEndSignal(shsurf);
            shsurf->m_runningGrab = nullptr;
            delete this;
        }
    }

    ShellSurface *shsurf;
    wl_listener shsurf_destroy_listener;
    wl_fixed_t dx, dy;
};

void ShellSurface::dragMove(struct weston_seat *ws)
{
    if (m_runningGrab) {
        return;
    }

    if (m_type == ShellSurface::Type::Fullscreen) {
        return;
    }

    MoveGrab *move = new MoveGrab;
    if (!move)
        return;

    move->dx = wl_fixed_from_double(m_view->geometry.x) - ws->pointer->grab_x;
    move->dy = wl_fixed_from_double(m_view->geometry.y) - ws->pointer->grab_y;
    move->shsurf = this;
    m_runningGrab = move;

    m_shell->startGrab(move, ws, DESKTOP_SHELL_CURSOR_MOVE);
    moveStartSignal(this);
}

// -- Resize --

class ResizeGrab : public ShellGrab
{
public:
    void motion(uint32_t time, wl_fixed_t x, wl_fixed_t y) override
    {
        weston_pointer_move(pointer(), x, y);

        if (!shsurf)
            return;

        weston_view *view = shsurf->m_view;

        wl_fixed_t from_x, from_y;
        wl_fixed_t to_x, to_y;
        weston_view_from_global_fixed(view, pointer()->grab_x, pointer()->grab_y, &from_x, &from_y);
        weston_view_from_global_fixed(view, pointer()->x, pointer()->y, &to_x, &to_y);

        int32_t w = width;
        if (edges & ShellSurface::Edges::Left) {
            w += wl_fixed_to_int(from_x - to_x);
        } else if (edges & ShellSurface::Edges::Right) {
            w += wl_fixed_to_int(to_x - from_x);
        }

        int32_t h = height;
        if (edges & ShellSurface::Edges::Top) {
            h += wl_fixed_to_int(from_y - to_y);
        } else if (edges & ShellSurface::Edges::Bottom) {
            h += wl_fixed_to_int(to_y - from_y);
        }

        shsurf->m_client->send_configure(shsurf->m_surface, (int)edges, w, h);
    }
    void button(uint32_t time, uint32_t button, uint32_t state) override
    {
        if (pointer()->button_count == 0 && state == WL_POINTER_BUTTON_STATE_RELEASED) {
            shsurf->m_runningGrab = nullptr;
            delete this;
        }
    }

    ShellSurface *shsurf;
    wl_listener shsurf_destroy_listener;
    ShellSurface::Edges edges;
    int32_t width, height;
};

void ShellSurface::dragResize(weston_seat *ws, Edges edges)
{
    if (m_runningGrab) {
        return;
    }

    ResizeGrab *grab = new ResizeGrab;
    if (!grab)
        return;

    int e = (int)edges;
    if (e == 0 || e > 15 || (e & 3) == 3 || (e & 12) == 12) {
        return;
    }

    grab->edges = edges;

    IRect2D rect = surfaceTreeBoundingBox();
    grab->width = rect.width;
    grab->height = rect.height;
    grab->shsurf = this;
    m_runningGrab = grab;

    m_shell->startGrab(grab, ws, e);
}

/*
 * Returns the bounding box of a surface and all its sub-surfaces,
 * in the surface coordinates system. */
IRect2D ShellSurface::surfaceTreeBoundingBox() const {
    pixman_region32_t region;
    pixman_box32_t *box;
    struct weston_subsurface *subsurface;

    pixman_region32_init_rect(&region, 0, 0,
                              m_surface->width,
                              m_surface->height);

    wl_list_for_each(subsurface, &m_surface->subsurface_list, parent_link) {
        pixman_region32_union_rect(&region, &region,
                                   subsurface->position.x,
                                   subsurface->position.y,
                                   subsurface->surface->width,
                                   subsurface->surface->height);
    }

    box = pixman_region32_extents(&region);
    IRect2D rect(box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
    pixman_region32_fini(&region);

    return rect;
}

void ShellSurface::setPopup(struct weston_surface *parent, weston_seat *seat, int32_t x, int32_t y, uint32_t serial)
{
    m_parent = parent;
    m_popup.x = x;
    m_popup.y = y;
    m_popup.serial = serial;
    m_popup.seat = ShellSeat::shellSeat(seat);

    m_pendingType = Type::Popup;
}

void ShellSurface::setMaximized(weston_output *out)
{
    m_output = out;
    uint32_t edges = (int)ShellSurface::Edges::TopLeft;

    IRect2D rect = Shell::instance()->windowsArea(m_output);
    m_client->send_configure(m_surface, edges, rect.width, rect.height);
    m_pendingType = Type::Maximized;
}
