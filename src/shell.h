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

#ifndef SHELL_H
#define SHELL_H

#include <list>
#include <vector>

#include "utils.h"
#include "layer.h"

struct weston_view;

class ShellSurface;
class Effect;
class Workspace;
class ShellSeat;
class Animation;

typedef std::list<ShellSurface *> ShellSurfaceList;

class Shell;

class ShellGrab {
public:
    ShellGrab();
    virtual ~ShellGrab();

    void end();

    Shell *shell() const { return m_shell; }
    weston_pointer *pointer() const { return m_pointer; }

    static ShellGrab *fromGrab(weston_pointer_grab *grab);

protected:
    virtual void focus() {}
    virtual void motion(uint32_t time, wl_fixed_t x, wl_fixed_t y) {}
    virtual void button(uint32_t time, uint32_t button, uint32_t state) {}
    virtual void cancel() {}

private:
    Shell *m_shell;
    weston_pointer *m_pointer;
    struct Grab {
        weston_pointer_grab base;
        ShellGrab *parent;
    } m_grab;

    static const weston_pointer_grab_interface s_shellGrabInterface;

    friend class Shell;
};

class Shell {
public:
    enum class PanelPosition {
        Top = 0,
        Left = 1,
        Bottom = 2,
        Right = 3
    };

    template<class T>
    static Shell *load(struct weston_compositor *ec, char *client);
    virtual ~Shell();

    void launchShellProcess();
    ShellSurface *createShellSurface(struct weston_surface *surface, const struct weston_shell_client *client);
    ShellSurface *getShellSurface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource);
    void removeShellSurface(ShellSurface *surface);
    static ShellSurface *getShellSurface(const struct weston_surface *surf);
    static weston_view *defaultView(const weston_surface *surface);

    void registerEffect(Effect *effect);

    void configureSurface(ShellSurface *surface, int32_t sx, int32_t sy, int32_t width, int32_t height);

    void setBackgroundSurface(struct weston_surface *surface, struct weston_output *output);
    void setGrabSurface(struct weston_surface *surface);
    void addPanelSurface(weston_surface *surface, weston_output *output, PanelPosition pos);
    void addOverlaySurface(struct weston_surface *surface, struct weston_output *output);

    void startGrab(ShellGrab *grab, weston_seat *seat, int32_t cursor = -1);

    void showPanels();
    void hidePanels();
    bool isInFullscreen() const;

    virtual IRect2D windowsArea(struct weston_output *output) const;

    inline struct weston_compositor *compositor() const { return m_compositor; }
    struct weston_output *getDefaultOutput() const;
    Workspace *currentWorkspace() const;
    Workspace *workspace(uint32_t id) const;
    void selectPreviousWorkspace();
    void selectNextWorkspace();
    void selectWorkspace(int32_t id);
    uint32_t numWorkspaces() const;

    void showAllWorkspaces();
    void resetWorkspaces();

    void minimizeWindows();
    void restoreWindows();

    struct wl_client *shellClient() { return m_child.client; }
    struct wl_resource *shellClientResource() { return m_child.desktop_shell; }

    static Shell *instance() { return s_instance; }

protected:
    Shell(struct weston_compositor *ec);
    virtual void init();
    void quit();
    inline const ShellSurfaceList &surfaces() const { return m_surfaces; }
    virtual void setGrabCursor(uint32_t cursor) {}
    virtual void setBusyCursor(ShellSurface *shsurf, struct weston_seat *seat) {}
    virtual void endBusyCursor(struct weston_seat *seat) {}
    void fadeSplash();
    void addWorkspace(Workspace *ws);
    virtual void panelConfigure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height, PanelPosition pos);

    virtual void defaultPointerGrabFocus(weston_pointer_grab *grab);
    virtual void defaultPointerGrabMotion(weston_pointer_grab *grab, uint32_t time, wl_fixed_t x, wl_fixed_t y);
    virtual void defaultPointerGrabButton(weston_pointer_grab *grab, uint32_t time, uint32_t button, uint32_t state);

    struct Child {
        Shell *shell;
        struct weston_process process;
        struct wl_client *client;
        struct wl_resource *desktop_shell;

        unsigned deathcount;
        uint32_t deathstamp;
    };
    Child m_child;

    Layer m_backgroundLayer;
    Layer m_panelsLayer;
    Layer m_fullscreenLayer;
    Layer m_overlayLayer;

private:
    void destroy();
    void bind(struct wl_client *client, uint32_t version, uint32_t id);
    void sigchld(int status);
    void backgroundConfigure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height);
    void activateSurface(struct weston_seat *seat, uint32_t time, uint32_t button);
    void configureFullscreen(ShellSurface *surface);
    void stackFullscreen(ShellSurface *surface);
    weston_view *createBlackSurface(ShellSurface *fs_surface, float x, float y, int w, int h);
    static void sendConfigure(struct weston_surface *surface, uint32_t edges, int32_t width, int32_t height);
    bool surfaceIsTopFullscreen(ShellSurface *surface);
    void activateWorkspace(Workspace *old);
    void pointerFocus(ShellSeat *shseat, struct weston_pointer *pointer);
    void pingTimeout(ShellSurface *shsurf);
    void pong(ShellSurface *shsurf);
    weston_view *createBlackSurface(int x, int y, int w, int h);
    void workspaceRemoved(Workspace *ws);

    struct weston_compositor *m_compositor;
    WlListener m_destroyListener;
    char *m_clientPath;
    Layer m_splashLayer;
    std::vector<Effect *> m_effects;
    ShellSurfaceList m_surfaces;
    std::vector<Workspace *> m_workspaces;
    uint32_t m_currentWorkspace;
    bool m_windowsMinimized;
    bool m_quitting;

    std::list<weston_view *> m_blackSurfaces;
    class Splash *m_splash;
    weston_view *m_grabSurface;

    static void staticPanelConfigure(weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height);

    static const struct wl_shell_interface shell_implementation;
    static const struct weston_shell_client shell_client;
    static const weston_pointer_grab_interface s_defaultPointerGrabInterface;
    static Shell *s_instance;

    friend class Effect;
};

template<class T>
Shell *Shell::load(struct weston_compositor *ec, char *client)
{
    Shell *shell = new T(ec);
    if (shell) {
        shell->m_clientPath = client;
        shell->init();
    }

    return shell;
}

#endif
