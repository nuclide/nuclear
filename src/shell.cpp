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
#include <stdlib.h>
#include <sys/time.h>
#include <linux/input.h>
#include <string.h>
#include <signal.h>

#include <wayland-server.h>

#include <weston/compositor.h>

#include "shell.h"
#include "shellsurface.h"
#include "shellseat.h"
#include "workspace.h"
#include "effect.h"
#include "desktop-shell.h"
#include "animation.h"

class Splash {
public:
    Splash() {}
    void addOutput(weston_view *s)
    {
        Animation *a = new Animation;
        splash spl{s, a};
        splashes.push_back(spl);
        a->updateSignal->connect(&splashes.back(), &splash::setAlpha);
    }
    void fadeOut()
    {
        for (splash &s: splashes) {
            s.fadeAnimation->setStart(1.f);
            s.fadeAnimation->setTarget(0.f);
            s.fadeAnimation->run(s.view->output, 200);
        }
    }

private:
    struct splash {
        weston_view *view;
        Animation *fadeAnimation;

        void setAlpha(float a)
        {
            view->alpha = a;
            weston_view_geometry_dirty(view);
            weston_surface_damage(view->surface);
        }
    };
    std::list<splash> splashes;
};

ShellGrab::ShellGrab()
         : m_shell(nullptr)
         , m_pointer(nullptr)
{
    m_grab.base.interface = &s_shellGrabInterface;
    m_grab.parent = this;
}

ShellGrab::~ShellGrab()
{
    end();
}

void ShellGrab::end()
{
    if (m_pointer) {
        weston_pointer_end_grab(m_pointer);
        m_pointer = nullptr;
    }
}

void ShellGrab::motion(uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    m_shell->movePointer(m_pointer, time, x, y);
}

ShellGrab *ShellGrab::fromGrab(weston_pointer_grab *grab)
{
    Grab *wrapper = container_of(grab, Grab, base);
    return wrapper->parent;
}

void ShellGrab::setCursor(uint32_t cursor)
{
    shell()->setGrabCursor(cursor);
    weston_pointer_set_focus(pointer(), shell()->m_grabSurface, wl_fixed_from_int(0), wl_fixed_from_int(0));
}

void ShellGrab::unsetCursor()
{
    wl_fixed_t sx, sy;
    weston_view *view = weston_compositor_pick_view(pointer()->seat->compositor, pointer()->x, pointer()->y, &sx, &sy);

    if (pointer()->focus != view) {
        weston_pointer_set_focus(pointer(), view, sx, sy);
    }
}

const weston_pointer_grab_interface ShellGrab::s_shellGrabInterface = {
    [](weston_pointer_grab *base)                                                 { ShellGrab::fromGrab(base)->focus(); },
    [](weston_pointer_grab *base, uint32_t time, wl_fixed_t x, wl_fixed_t y)      { ShellGrab::fromGrab(base)->motion(time, x, y); },
    [](weston_pointer_grab *base, uint32_t time, uint32_t button, uint32_t state) { ShellGrab::fromGrab(base)->button(time, button, state); },
    [](weston_pointer_grab *base)                                                 { ShellGrab::fromGrab(base)->cancel(); }
};


Shell *Shell::s_instance = nullptr;

static void black_surface_configure(weston_surface *es, int32_t sx, int32_t sy)
{
}

void Shell::defaultPointerGrabFocus(weston_pointer_grab *grab)
{
    weston_pointer *pointer = grab->pointer;

    if (pointer->button_count > 0) {
        return;
    }

    wl_fixed_t sx, sy;
    weston_view *view = weston_compositor_pick_view(pointer->seat->compositor, pointer->x, pointer->y, &sx, &sy);

    if (view && view->surface->configure == &black_surface_configure) {
        view = static_cast<ShellSurface *>(view->surface->configure_private)->view();
    }
    if (pointer->focus != view) {
        weston_pointer_set_focus(pointer, view, sx, sy);
    }
}

void Shell::defaultPointerGrabMotion(weston_pointer_grab *grab, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    weston_pointer *pointer = grab->pointer;

    movePointer(pointer, time, x, y);

    wl_list *resource_list = &pointer->focus_resource_list;
    wl_resource *resource;
    wl_resource_for_each(resource, resource_list) {
        wl_fixed_t sx, sy;
        weston_view_from_global_fixed(pointer->focus, pointer->x, pointer->y, &sx, &sy);
        wl_pointer_send_motion(resource, time, sx, sy);
    }
}

void Shell::defaultPointerGrabButton(weston_pointer_grab *grab, uint32_t time, uint32_t button, uint32_t state_w)
{
    weston_pointer *pointer = grab->pointer;
    weston_compositor *compositor = pointer->seat->compositor;

    wl_list *resource_list = &pointer->focus_resource_list;
    if (!wl_list_empty(resource_list)) {
        wl_display *display = compositor->wl_display;
        wl_resource *resource;
        uint32_t serial = wl_display_next_serial(display);
        wl_resource_for_each(resource, resource_list) {
            wl_pointer_send_button(resource, serial, time, button, state_w);
        }
    }

    if (pointer->button_count == 0 && state_w == WL_POINTER_BUTTON_STATE_RELEASED) {
        wl_fixed_t sx, sy;
        weston_view *view = weston_compositor_pick_view(compositor, pointer->x, pointer->y, &sx, &sy);
        weston_pointer_set_focus(pointer, view, sx, sy);
    }
}

void Shell::movePointer(weston_pointer *pointer, uint32_t time, wl_fixed_t fx, wl_fixed_t fy)
{
    weston_pointer_move(pointer, fx, fy);

    if (time - m_lastMotionTime < 1000) {
        return;
    }

    int x = wl_fixed_to_int(fx);
    int y = wl_fixed_to_int(fy);
    weston_output *tmp = nullptr, *out = nullptr;
    wl_list_for_each(tmp, &m_compositor->output_list, link) {
        if (pixman_region32_contains_point(&tmp->region, x, y, NULL)) {
            out = tmp;
            break;
        }
    }
    if (!out) {
        out = getDefaultOutput();
    }

    const int pushTime = 150;
    Binding::HotSpot hs;
    bool inHs = true;
    if (x <= out->x && y <= out->y) {
        hs = Binding::HotSpot::TopLeftCorner;
    } else if (x >= out->x + out->width - 1 && y <= out->y) {
        hs = Binding::HotSpot::TopRightCorner;
    } else if (x <= out->x && y >= out->y + out->height - 1) {
        hs = Binding::HotSpot::BottomLeftCorner;
    } else if (x >= out->x + out->width - 1 && y >= out->y + out->height - 1) {
        hs = Binding::HotSpot::BottomRightCorner;
    } else {
        inHs = false;
        m_enterHotZone = 0;
    }

    if (inHs) {
        if (m_enterHotZone == 0) {
            m_enterHotZone = time;
        } else if (time - m_enterHotZone > pushTime) {
            m_lastMotionTime = time;
            for (Binding *b: m_hotSpotBindings[(int)hs]) {
                b->hotSpotHandler(pointer->seat, time, hs);
            }
        }
    }
}

static void default_grab_pointer_cancel(weston_pointer_grab *grab) {}

static Shell *shellFromPointer(weston_pointer_grab *grab)
{
    return static_cast<Shell *>(grab->pointer->seat->compositor->shell_interface.shell);
}

const weston_pointer_grab_interface Shell::s_defaultPointerGrabInterface = {
    [](weston_pointer_grab *g)                                                   { shellFromPointer(g)->defaultPointerGrabFocus(g); },
    [](weston_pointer_grab *g, uint32_t time, wl_fixed_t x, wl_fixed_t y)        { shellFromPointer(g)->defaultPointerGrabMotion(g, time, x, y); },
    [](weston_pointer_grab *g, uint32_t time, uint32_t button, uint32_t state_w) { shellFromPointer(g)->defaultPointerGrabButton(g, time, button, state_w); },
    default_grab_pointer_cancel,
};


Shell::Shell(struct weston_compositor *ec)
            : m_compositor(ec)
            , m_windowsMinimized(false)
            , m_quitting(false)
            , m_background(nullptr)
            , m_lastMotionTime(0)
            , m_enterHotZone(0)
            , m_grabSurface(nullptr)
{
    s_instance = this;

    srandom(weston_compositor_get_time());
    m_child.shell = this;
    m_child.deathstamp = 0;
}

Shell::~Shell()
{
    free(m_clientPath);
    if (m_child.client) {
        kill(m_child.process.pid, SIGKILL);
    }
}

void Shell::destroy()
{
    delete this;
}

void Shell::init()
{
#define _this reinterpret_cast<ShellSurface *>(shsurf)
    m_compositor->shell_interface.shell = this;
    m_compositor->shell_interface.create_shell_surface = [](void *shell, weston_surface *surface, const weston_shell_client *client) {
        return (shell_surface *)static_cast<Shell *>(shell)->createShellSurface(surface, client);
    };
    m_compositor->shell_interface.set_toplevel = [](shell_surface *shsurf) { _this->setTopLevel(); };
    m_compositor->shell_interface.set_transient = [](shell_surface *shsurf, weston_surface *parent, int x, int y, uint32_t flags) { _this->setTransient(parent, x, y, flags); };
    m_compositor->shell_interface.set_fullscreen = [](shell_surface *shsurf, uint32_t method, uint32_t framerate, weston_output *output) { _this->setFullscreen(method, framerate, output);};
    m_compositor->shell_interface.resize = [](shell_surface *shsurf, weston_seat *ws, uint32_t edges) { _this->dragResize(ws, edges); return 0; };
    m_compositor->shell_interface.move = [](shell_surface *shsurf, weston_seat *ws) { _this->dragMove(ws); return 0; };
    m_compositor->shell_interface.set_xwayland = [](shell_surface *shsurf, int x, int y, uint32_t flags) { _this->setXWayland(x, y, flags); };
    m_compositor->shell_interface.set_title = [](shell_surface *shsurf, const char *t) { _this->setTitle(t); };
#undef _this
    weston_compositor_set_default_pointer_grab(m_compositor, &s_defaultPointerGrabInterface);

    m_destroyListener.listen(&m_compositor->destroy_signal);
    m_destroyListener.signal->connect(this, &Shell::destroy);

    if (!wl_global_create(m_compositor->wl_display, &wl_shell_interface, 1, this,
        [](struct wl_client *client, void *data, uint32_t version, uint32_t id) { static_cast<Shell *>(data)->bind(client, version, id); }))
        return;

    struct weston_seat *seat;
    wl_list_for_each(seat, &m_compositor->seat_list, link) {
        ShellSeat *shseat = ShellSeat::shellSeat(seat);
        shseat->pointerFocusSignal.connect(this, &Shell::pointerFocus);
    }

    m_splashLayer.insert(&m_compositor->cursor_layer);
    m_overlayLayer.insert(&m_splashLayer);
    m_fullscreenLayer.insert(&m_overlayLayer);
    m_panelsLayer.insert(&m_fullscreenLayer);
    m_limboLayer.insert(&m_panelsLayer);
    m_backgroundLayer.insert(&m_limboLayer);

    m_currentWorkspace = 0;
    m_splash = new Splash;

    struct weston_output *out;
    wl_list_for_each(out, &m_compositor->output_list, link) {
        int x = out->x, y = out->y;
        int w = out->width, h = out->height;

        weston_view *blackSurface = createBlackSurface(x, y, w, h);
        m_backgroundLayer.addSurface(blackSurface);
        m_blackSurfaces.push_back(blackSurface);

        weston_view *splashSurface = createBlackSurface(x, y, w, h);
        m_splashLayer.addSurface(splashSurface);
        m_splash->addOutput(splashSurface);
    }

    struct wl_event_loop *loop = wl_display_get_event_loop(m_compositor->wl_display);
    wl_event_loop_add_idle(loop, [](void *data) { static_cast<Shell *>(data)->launchShellProcess(); }, this);

    weston_compositor_add_button_binding(compositor(), BTN_LEFT, (weston_keyboard_modifier)0,
                                         [](struct weston_seat *seat, uint32_t time, uint32_t button, void *data) {
                                             static_cast<Shell *>(data)->activateSurface(seat, time, button); }, this);

    weston_compositor_add_key_binding(compositor(), KEY_LEFT, MODIFIER_CTRL, [](struct weston_seat *seat, uint32_t time, uint32_t key, void *data) {
                                            static_cast<Shell *>(data)->selectPreviousWorkspace(); }, this);
    weston_compositor_add_key_binding(compositor(), KEY_RIGHT, MODIFIER_CTRL, [](struct weston_seat *seat, uint32_t time, uint32_t key, void *data) {
                                            static_cast<Shell *>(data)->selectNextWorkspace(); }, this);
}

void Shell::addWorkspace(Workspace *ws)
{
    if (m_background) {
        ws->createBackgroundView(m_background);
    }
    m_workspaces.push_back(ws);
    ws->destroyedSignal.connect(this, &Shell::workspaceRemoved);
    if (ws->number() == 0) {
        activateWorkspace(nullptr);
    }
}

void Shell::workspaceRemoved(Workspace *ws)
{
    for (auto i = m_workspaces.begin(); i != m_workspaces.end(); ++i) {
        if (*i == ws) {
            m_workspaces.erase(i);
            break;
        }
    }

    int nextWs = ws->number();
    if (nextWs >= (int)m_workspaces.size()) {
        nextWs = m_workspaces.size() - 1;
    }
    if (ws->active()) {
        m_currentWorkspace = nextWs;
    }

    for (const weston_view *s: ws->layer()) {
        ShellSurface *shsurf = Shell::getShellSurface(s->surface);
        if (!shsurf)
            continue;

        workspace(nextWs)->addSurface(shsurf);
        shsurf->m_workspace = workspace(nextWs);
    }

    activateWorkspace(nullptr);
}

weston_view *Shell::createBlackSurface(int x, int y, int w, int h)
{
    weston_surface *surface = weston_surface_create(m_compositor);
    weston_view *view = weston_view_create(surface);

    surface->configure = black_surface_configure;
    surface->configure_private = 0;
    weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1);
    pixman_region32_fini(&surface->input);
    pixman_region32_init_rect(&surface->input, 0, 0, 0, 0);

    surface->width = w;
    surface->height = h;
    weston_view_set_position(view, x, y);

    return view;
}

void Shell::quit()
{
    m_quitting = true;
    wl_display_terminate(compositor()->wl_display);
    if (m_child.client) {
        wl_client_destroy(m_child.client);
        kill(m_child.process.pid, SIGTERM);
    }
}

weston_view *Shell::defaultView(const weston_surface *surface)
{
    if (!surface || wl_list_empty(&surface->views)) {
            return nullptr;
    }

    if (ShellSurface *shsurf = getShellSurface(surface)) {
        return shsurf->view();
    }

    weston_view *view;
    wl_list_for_each(view, &surface->views, surface_link) {
        if (weston_view_is_mapped(view)) {
            return view;
        }
    }

    return container_of(surface->views.next, weston_view, surface_link);
}

void Shell::configureSurface(ShellSurface *surface, int32_t sx, int32_t sy)
{
    if (surface->width() == 0) {
        surface->unmapped();
        return;
    }

    if (surface->m_type == ShellSurface::Type::Fullscreen && surface->m_pendingType != ShellSurface::Type::Fullscreen &&
        surface->m_pendingType != ShellSurface::Type::None) {
        if (surface->m_fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER && surfaceIsTopFullscreen(surface)) {
            weston_output_switch_mode(surface->m_fullscreen.output, surface->m_fullscreen.output->original_mode, surface->m_fullscreen.output->original_scale, WESTON_MODE_SWITCH_RESTORE_NATIVE);
        }
    }
    bool changedType = surface->updateType();

    if (!surface->isMapped()) {
        switch (surface->m_type) {
            case ShellSurface::Type::TopLevel:
                surface->map(10 + random() % 400, 10 + random() % 400);
                break;
            default:
                surface->map(surface->view()->geometry.x + sx, surface->view()->geometry.y + sy);
        }

        for (Effect *e: m_effects) {
            e->addSurface(surface);
        }

        switch (surface->m_type) {
            case ShellSurface::Type::Transient:
            case ShellSurface::Type::Popup:
                if (ShellSurface *p = getShellSurface(surface->m_parent)) {
                    surface->m_workspace = p->m_workspace;
                } else {
                    surface->m_workspace = 0;
                }
                surface->view()->output = surface->m_parent->output;
		wl_list_insert(defaultView(surface->m_parent)->layer_link.prev, &surface->view()->layer_link);
                break;
            case ShellSurface::Type::Fullscreen:
                stackFullscreen(surface);
                configureFullscreen(surface);
                break;
            case ShellSurface::Type::None:
                break;
            default:
                surface->m_workspace->addSurface(surface);
                m_surfaces.push_back(surface);
        }

        switch (surface->m_type) {
            case ShellSurface::Type::XWayland:
            case ShellSurface::Type::Transient:
                if (surface->m_transient.flags == WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
                    break;
                }
            case ShellSurface::Type::TopLevel:
            case ShellSurface::Type::Maximized: {
                struct weston_seat *seat;
                wl_list_for_each(seat, &m_compositor->seat_list, link) {
                    ShellSeat::shellSeat(seat)->activate(surface);
                }
            } break;
            default:
                break;
        }

        if (m_windowsMinimized) {
            switch (surface->m_type) {
                case ShellSurface::Type::Transient:
                case ShellSurface::Type::Popup:
                    if (surface->m_transient.flags == WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
                        break;
                    }
                default:
                    surface->hide();
            }
        }
    } else if (changedType || sx != 0 || sy != 0 || surface->width() != surface->m_lastWidth || surface->height() != surface->m_lastHeight) {
        surface->m_lastWidth = surface->width();
        surface->m_lastHeight = surface->height();
        float from_x, from_y;
        float to_x, to_y;

        weston_view *view = surface->view();
        weston_view_to_global_float(view, 0, 0, &from_x, &from_y);
        weston_view_to_global_float(view, sx, sy, &to_x, &to_y);
        int x = surface->x() + to_x - from_x;
        int y = surface->y() + to_y - from_y;

        weston_view_set_position(view, x, y);

        switch (surface->m_type) {
            case ShellSurface::Type::Fullscreen:
                stackFullscreen(surface);
                configureFullscreen(surface);
                break;
            case ShellSurface::Type::Maximized: {
                IRect2D rect = windowsArea(surface->output());
                IRect2D bbox = surface->surfaceTreeBoundingBox();
                weston_view_set_position(view, rect.x - bbox.x, rect.y - bbox.y);
            } break;
            default:
                if (!m_windowsMinimized) {
                    surface->m_workspace->addSurface(surface);
                }
                break;
        }

        if (surface->m_surface->output) {
            weston_view_update_transform(view);

            if (surface->m_type == ShellSurface::Type::Maximized) {
                surface->m_surface->output = surface->m_output;
            }
        }
    }
}

weston_view *Shell::createBlackSurface(ShellSurface *fs_surface, float x, float y, int w, int h)
{
    weston_surface *surface = weston_surface_create(m_compositor);
    if (!surface) {
        weston_log("no memory\n");
        return nullptr;
    }
    weston_view *view = weston_view_create(surface);

    surface->configure = black_surface_configure;
    surface->configure_private = fs_surface;
    weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1);
    pixman_region32_fini(&surface->opaque);
    pixman_region32_init_rect(&surface->opaque, 0, 0, w, h);
    pixman_region32_fini(&surface->input);
    pixman_region32_init_rect(&surface->input, 0, 0, w, h);

    surface->width = w;
    surface->height = h;
    weston_view_set_position(view, x, y);

    return view;
}

void Shell::configureFullscreen(ShellSurface *shsurf)
{
    struct weston_output *output = shsurf->m_fullscreen.output;
    struct weston_surface *surface = shsurf->m_surface;
    weston_view *view = shsurf->view();

    if (!shsurf->m_fullscreen.blackView) {
        shsurf->m_fullscreen.blackView = createBlackSurface(shsurf, output->x, output->y, output->width, output->height);
    }

    m_fullscreenLayer.stackBelow(shsurf->m_fullscreen.blackView, shsurf->view());

    IRect2D bbox = shsurf->surfaceTreeBoundingBox();

    switch (shsurf->m_fullscreen.type) {
    case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT:
        if (surface->buffer_ref.buffer) {
            shsurf->centerOnOutput(shsurf->m_fullscreen.output);
        } break;
    case WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE: {
        /* 1:1 mapping between surface and output dimensions */
        if (output->width == bbox.width && output->height == bbox.height) {
            weston_view_set_position(view, output->x - bbox.x, output->y - bbox.y);
            break;
        }

        struct weston_matrix *matrix = &shsurf->m_fullscreen.transform.matrix;
        weston_matrix_init(matrix);

        float output_aspect = (float) output->width / (float) output->height;
        float surface_aspect = (float) surface->width / (float) surface->height;
        float scale;
        if (output_aspect < surface_aspect) {
            scale = (float) output->width / (float) bbox.width;
        } else {
            scale = (float) output->height / (float) bbox.height;
        }

        weston_matrix_scale(matrix, scale, scale, 1);
        shsurf->addTransform(&shsurf->m_fullscreen.transform);
        float x = output->x + (output->width - bbox.width * scale) / 2 - bbox.x;
        float y = output->y + (output->height - bbox.height * scale) / 2 - bbox.y;
        weston_view_set_position(view, x, y);

    } break;
    case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER:
        if (surfaceIsTopFullscreen(shsurf)) {
            int32_t scale = surface->buffer_viewport.scale;
            weston_mode mode = {0, bbox.width * scale, bbox.height * scale, shsurf->m_fullscreen.framerate, { nullptr, nullptr } };

            if (weston_output_switch_mode(output, &mode, scale, WESTON_MODE_SWITCH_SET_TEMPORARY) == 0) {
                weston_view_set_position(view, output->x - bbox.x, output->y - bbox.y);
                shsurf->m_fullscreen.blackView->surface->width = output->width;
                shsurf->m_fullscreen.blackView->surface->height = output->height;
                weston_view_set_position(shsurf->m_fullscreen.blackView, output->x - bbox.x, output->y - bbox.y);
                break;
            }
        }
        break;
    case WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL:
        break;
    default:
        break;
    }
}

void Shell::stackFullscreen(ShellSurface *shsurf)
{
    m_fullscreenLayer.addSurface(shsurf);
    weston_surface_damage(shsurf->m_surface);

    if (!shsurf->m_fullscreen.blackView) {
        struct weston_output *output = shsurf->m_fullscreen.output;
        shsurf->m_fullscreen.blackView = createBlackSurface(shsurf, output->x, output->y, output->width, output->height);
    }

    m_fullscreenLayer.stackBelow(shsurf->m_fullscreen.blackView, shsurf->view());
    weston_surface_damage(shsurf->m_fullscreen.blackView->surface);
}

bool Shell::surfaceIsTopFullscreen(ShellSurface *surface)
{
    if (m_fullscreenLayer.isEmpty()) {
        return false;
    }

    weston_view *top = *m_fullscreenLayer.begin();
    return top == surface->view();
}

bool Shell::isInFullscreen() const
{
    return m_fullscreenLayer.isVisible();
}

static void shell_surface_configure(struct weston_surface *surf, int32_t sx, int32_t sy)
{
    ShellSurface *shsurf = Shell::getShellSurface(surf);
    if (shsurf) {
        shsurf->shell()->configureSurface(shsurf, sx, sy);
    }
}

ShellSurface *Shell::createShellSurface(struct weston_surface *surface, const struct weston_shell_client *client)
{
    ShellSurface *shsurf = new ShellSurface(this, surface);

    surface->configure = shell_surface_configure;
    surface->configure_private = shsurf;
    shsurf->m_client = client;
    shsurf->m_workspace = currentWorkspace();
    return shsurf;
}

void Shell::sendConfigure(struct weston_surface *surface, uint32_t edges, int32_t width, int32_t height)
{
    ShellSurface *shsurf = Shell::getShellSurface(surface);
    wl_shell_surface_send_configure(shsurf->m_resource, edges, width, height);
}

const struct weston_shell_client Shell::shell_client = {
    Shell::sendConfigure
};

ShellSurface *Shell::getShellSurface(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                                     struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<weston_surface *>(wl_resource_get_user_data(surface_resource));

    ShellSurface *shsurf = getShellSurface(surface);
    if (shsurf) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "desktop_shell::get_shell_surface already requested");
        return shsurf;
    }

    shsurf = createShellSurface(surface, &shell_client);
    if (!shsurf) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface->configure already set");
        return nullptr;
    }

    shsurf->init(client, id);
    shsurf->pingTimeoutSignal.connect(this, &Shell::pingTimeout);

    return shsurf;
}

ShellSurface *Shell::getShellSurface(const struct weston_surface *surf)
{
    if (surf->configure == shell_surface_configure) {
        return static_cast<ShellSurface *>(surf->configure_private);
    }

    return nullptr;
}

void Shell::removeShellSurface(ShellSurface *surface)
{
    for (Effect *e: m_effects) {
        e->removeSurface(surface);
    }
    m_surfaces.remove(surface);
}

void Shell::registerEffect(Effect *effect)
{
    m_effects.push_back(effect);
    for (ShellSurface *s: m_surfaces) {
        effect->addSurface(s);
    }
}

void Shell::activateSurface(struct weston_seat *seat, uint32_t time, uint32_t button)
{
    weston_view *focus = seat->pointer->focus;
    if (!focus)
        return;

    if (seat->pointer->grab == &seat->pointer->default_grab) {
        ShellSurface *shsurf = getShellSurface(focus->surface);
        if (shsurf && shsurf->type() == ShellSurface::Type::Fullscreen) {
            return;
        }

        ShellSeat *shseat = ShellSeat::shellSeat(seat);
        if (shsurf) {
            shseat->activate(shsurf);
        } else {
            // Dont't activate the nullptr ShellSurface, as that would call setActive(false) on the previous
            // active ShellSurface, and that would break minimizing windows by clicking on the taskbar:
            // a click on the taskbar would deactivate the previous active surface, and the taskbar would
            // activate instead of minimizing it.
            weston_surface_activate(focus->surface, seat);
        }
    };
}

void Shell::startGrab(ShellGrab *grab, weston_seat *seat, int32_t cursor)
{
    ShellSeat::shellSeat(seat)->endPopupGrab();

    grab->m_shell = this;
    weston_pointer *pointer = seat->pointer;
    grab->m_pointer = pointer;

    weston_pointer_start_grab(pointer, &grab->m_grab.base);
    if (cursor != -1) {
        setGrabCursor(cursor);
        weston_pointer_set_focus(pointer, m_grabSurface, wl_fixed_from_int(0), wl_fixed_from_int(0));
    }
}

static void configure_static_surface(struct weston_surface *es, Layer *layer)
{
    if (es->width == 0)
        return;

    weston_view *view = container_of(es->views.next, weston_view, surface_link);

    weston_view_set_position(view, view->geometry.x, view->geometry.y);

    if (wl_list_empty(&view->layer_link) || view->layer_link.next == view->layer_link.prev) {
        layer->addSurface(view);
        weston_compositor_schedule_repaint(es->compositor);
    }
}

void Shell::backgroundConfigure(struct weston_surface *es, int32_t sx, int32_t sy)
{
}

void Shell::panelConfigure(weston_surface *es, int32_t sx, int32_t sy, PanelPosition pos)
{
    if (es->width == 0)
        return;

    weston_output *output = es->output;
    weston_view *view = container_of(es->views.next, weston_view, surface_link);

    int32_t x = 0, y = 0;
    if (output) {
        switch (pos) {
            case PanelPosition::Top:
            case PanelPosition::Left:
                x = output->x;
                y = output->y;
                break;
            case PanelPosition::Right:
                y = output->y;
                x = output->x + output->width - es->width;
                break;
            case PanelPosition::Bottom:
                x = output->x;
                y = output->y + output->height - es->height;
                break;
        }
    }
    weston_view_set_position(view, x, y);

    if (wl_list_empty(&view->layer_link) || view->layer_link.next == view->layer_link.prev) {
        m_panelsLayer.addSurface(view);
        weston_compositor_schedule_repaint(es->compositor);
    }
}

void Shell::setBackgroundSurface(struct weston_surface *surface, struct weston_output *output)
{
    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy) {
        static_cast<Shell *>(es->configure_private)->backgroundConfigure(es, sx, sy); };
    surface->configure_private = this;
    surface->output = output;
    m_background = surface;

    for (Workspace *w: m_workspaces) {
        w->createBackgroundView(surface);
    }
}

void Shell::setGrabSurface(struct weston_surface *surface)
{
    if (m_grabSurface) {
        if (surface == m_grabSurface->surface) {
            return;
        }

        weston_view_destroy(m_grabSurface);
    }

    m_grabSurface = weston_view_create(surface);
}

struct Panel {
    Panel(weston_surface *s, Shell::PanelPosition p, Shell *sh)
        : surface(s), pos(p), shell(sh) {}
    weston_surface *surface;
    Shell::PanelPosition pos;
    Shell *shell;
    wl_listener destroyListener;
};

static void panelDestroyed(wl_listener *listener, void *data)
{
    weston_surface *surface = static_cast<weston_surface *>(data);
    delete static_cast<Panel *>(surface->configure_private);
}

void Shell::staticPanelConfigure(weston_surface *es, int32_t sx, int32_t sy) {
    Panel *p = static_cast<Panel *>(es->configure_private);
    p->shell->panelConfigure(es, sx, sy, p->pos);
}

void Shell::addPanelSurface(weston_surface *surface, weston_output *output, PanelPosition pos)
{
    if (surface->configure == staticPanelConfigure) {
        Panel *p = static_cast<Panel *>(surface->configure_private);
        p->pos = pos;
        surface->output = output;
        return;
    }

    Panel *panel = new Panel(surface, pos, this);
    surface->configure = staticPanelConfigure;
    surface->configure_private = panel;
    surface->output = output;
    weston_view_create(surface);

    panel->destroyListener.notify = panelDestroyed;
    wl_signal_add(&surface->destroy_signal, &panel->destroyListener);
}

void Shell::addOverlaySurface(struct weston_surface *surface, struct weston_output *output)
{
    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy) {
        configure_static_surface(es, &static_cast<Shell *>(es->configure_private)->m_overlayLayer); };
    surface->configure_private = this;
    surface->output = output;
    weston_view *view = weston_view_create(surface);
    weston_view_set_position(view, output->x, output->y);
}

void Shell::showPanels()
{
    for (weston_view *v: m_panelsLayer) {
        v->alpha = 1;
    }
}

void Shell::hidePanels()
{
    // Use this total transparency instead of removing the layer
    // because we want to the surface to keep receiving fram callbacks,
    // otherwise Qt's main thread will be stuck.
    for (weston_view *v: m_panelsLayer) {
        v->alpha = 0;
    }
}

IRect2D Shell::windowsArea(struct weston_output *output) const
{
    pixman_region32_t area;
    pixman_region32_init_rect(&area, output->x, output->y, output->width, output->height);

    for (weston_view *view: m_panelsLayer) {
        weston_surface *surface = view->surface;
        if (surface->output == output && surface->configure == staticPanelConfigure) {
            pixman_region32_t surf;
            pixman_region32_init(&surf);
            pixman_region32_copy(&surf, &surface->input);
            pixman_region32_translate(&surf, view->geometry.x, view->geometry.y);
            pixman_region32_subtract(&area, &area, &surf);
            pixman_region32_fini(&surf);
        }
    }
    pixman_box32_t *box = pixman_region32_extents(&area);
    pixman_region32_fini(&area);
    return IRect2D(box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
}

struct weston_output *Shell::getDefaultOutput() const
{
    return container_of(m_compositor->output_list.next, struct weston_output, link);
}

Workspace *Shell::currentWorkspace() const
{
    return m_workspaces[m_currentWorkspace];
}

Workspace *Shell::workspace(uint32_t id) const
{
    if (id >= m_workspaces.size()) {
        return nullptr;
    }

    return m_workspaces[id];
}

void Shell::selectPreviousWorkspace()
{
    Workspace *old = currentWorkspace();
    int i = m_currentWorkspace;
    if (--i < 0) {
        i = m_workspaces.size() - 1;
    }
    m_currentWorkspace = i;
    activateWorkspace(old);
}

void Shell::selectNextWorkspace()
{
    Workspace *old = currentWorkspace();
    if (++m_currentWorkspace == m_workspaces.size()) {
        m_currentWorkspace = 0;
    }
    activateWorkspace(old);
}

void Shell::selectWorkspace(int32_t id)
{
    if (id >= (int32_t)m_workspaces.size()) {
        return;
    }

    Workspace *old = currentWorkspace();

    if (id < 0) {
        old->remove();
        return;
    }
    m_currentWorkspace = id;
    activateWorkspace(old);
}

void Shell::activateWorkspace(Workspace *old)
{
    if (old) {
        old->setActive(false);
        old->remove();
    }

    currentWorkspace()->setActive(true);
    currentWorkspace()->insert(&m_limboLayer);

    for (const weston_view *view: currentWorkspace()->layer()) {
        ShellSurface *shsurf = getShellSurface(view->surface);
        if (shsurf) {
            weston_seat *seat;
            wl_list_for_each(seat, &m_compositor->seat_list, link) {
                ShellSeat::shellSeat(seat)->activate(shsurf);
            }
            return;
        }
    }
    weston_seat *seat;
    wl_list_for_each(seat, &m_compositor->seat_list, link) {
        ShellSeat::shellSeat(seat)->activate((weston_surface *)nullptr);
    }
}

uint32_t Shell::numWorkspaces() const
{
    return m_workspaces.size();
}

void Shell::showAllWorkspaces()
{
    currentWorkspace()->remove();

    Workspace *prev = nullptr;
    for (Workspace *w: m_workspaces) {
        if (prev) {
            w->insert(prev);
        } else {
            w->insert(&m_limboLayer);
        }
        prev = w;
    }
}

void Shell::resetWorkspaces()
{
    for (Workspace *w: m_workspaces) {
        w->remove();
    }
    activateWorkspace(nullptr);
}

void Shell::minimizeWindows()
{
    for (ShellSurface *shsurf: surfaces()) {
        switch (shsurf->m_type) {
            case ShellSurface::Type::Transient:
            case ShellSurface::Type::Popup:
                if (shsurf->m_transient.flags == WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
                    break;
                }
            default:
                if (!shsurf->isMinimized()) {
                    shsurf->minimize();
                }
                shsurf->setAcceptNewState(false);
        }
    }
    m_windowsMinimized = true;
}

void Shell::restoreWindows()
{
    for (ShellSurface *shsurf: surfaces()) {
        switch (shsurf->m_type) {
            case ShellSurface::Type::Transient:
            case ShellSurface::Type::Popup:
                if (shsurf->m_transient.flags == WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
                    break;
                }
            default:
                if (!shsurf->isMinimized()) {
                    shsurf->unminimize();
                }
                shsurf->setAcceptNewState(true);
        }
    }
    m_windowsMinimized = false;
}

void Shell::pointerFocus(ShellSeat *, struct weston_pointer *pointer)
{
    weston_view *view = pointer->focus;

    if (!view)
        return;

    weston_compositor *compositor = view->surface->compositor;
    ShellSurface *shsurf = Shell::getShellSurface(view->surface);
    if (!shsurf)
        return;

    if (!shsurf->isResponsive()) {
        shsurf->shell()->setBusyCursor(shsurf, pointer->seat);
    } else {
        uint32_t serial = wl_display_next_serial(compositor->wl_display);
        shsurf->ping(serial);
    }
}

void Shell::pingTimeout(ShellSurface *shsurf)
{
    struct weston_seat *seat;
    wl_list_for_each(seat, &m_compositor->seat_list, link) {
        if (seat->pointer->focus == shsurf->view())
            setBusyCursor(shsurf, seat);
    }
}

void Shell::pong(ShellSurface *shsurf)
{
    if (!shsurf->isResponsive()) {
        struct weston_seat *seat;
        /* Received pong from previously unresponsive client */
        wl_list_for_each(seat, &m_compositor->seat_list, link) {
            struct weston_pointer *pointer = seat->pointer;
            if (pointer) {
                endBusyCursor(seat);
            }
        }
    }
}

void Shell::fadeSplash()
{
    m_splash->fadeOut();
}

void Shell::bindHotSpot(Binding::HotSpot hs, Binding *b)
{
    m_hotSpotBindings[(int)hs].push_back(b);
}

void Shell::removeHotSpotBinding(Binding *b)
{
    for (auto v: m_hotSpotBindings) {
        v.second.remove(b);
    }
}

void Shell::putInLimbo(ShellSurface *s)
{
    m_limboLayer.addSurface(s);
}

const struct wl_shell_interface Shell::shell_implementation = {
    [](struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource) {
        static_cast<Shell *>(wl_resource_get_user_data(resource))->getShellSurface(client, resource, id, surface_resource);
    }
};

void Shell::bind(struct wl_client *client, uint32_t version, uint32_t id)
{
    struct wl_resource *resource = wl_resource_create(client, &wl_shell_interface, version, id);
    if (resource)
        wl_resource_set_implementation(resource, &shell_implementation, this, nullptr);
}

void Shell::sigchld(int status)
{
    uint32_t time;

    m_child.process.pid = 0;
    m_child.client = nullptr; /* already destroyed by wayland */

    if (m_quitting) {
        return;
    }

    /* if desktop-shell dies more than 5 times in 30 seconds, give up */
    time = weston_compositor_get_time();
    if (time - m_child.deathstamp > 30000) {
        m_child.deathstamp = time;
        m_child.deathcount = 0;
    }

    m_child.deathcount++;
    if (m_child.deathcount > 5) {
        weston_log("weston-desktop-shell died, giving up.\n");
        return;
    }

    weston_log("weston-desktop-shell died, respawning...\n");
    launchShellProcess();
}

void Shell::launchShellProcess()
{
    m_child.client = weston_client_launch(m_compositor,
                                          &m_child.process,
                                          m_clientPath,
                                          [](struct weston_process *process, int status) {
                                              Child *child = container_of(process, Child, process);
                                              child->shell->sigchld(status);
                                          });

    if (!m_child.client)
        weston_log("not able to start %s\n", m_clientPath);
}


WL_EXPORT int
module_init(struct weston_compositor *ec, int *argc, char *argv[])
{

    char *client = nullptr;

    for (int i = 0; i < *argc; ++i) {
        if (char *s = strstr(argv[i], "--nuclear-client=")) {
            client = strdup(s + 17);
            --*argc;
            break;
        }
    }

    Shell *shell = Shell::load<DesktopShell>(ec, client);
    if (!shell) {
        return -1;
    }

    return 0;
}
