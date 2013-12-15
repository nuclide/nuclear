

pkg_check_modules(WaylandServer wayland-server REQUIRED)
pkg_check_modules(Pixman pixman-1 REQUIRED)
pkg_check_modules(Weston weston REQUIRED)

include_directories(
    ${WaylandServer_INCLUDE_DIRS}
    ${Pixman_INCLUDE_DIRS}
    ${Weston_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/nuclear-shell")

set(SOURCES
    shell.cpp
    layer.cpp
    shellsurface.cpp
    shellseat.cpp
    workspace.cpp
    effect.cpp
    transform.cpp
    animation.cpp
    inputpanel.cpp
    binding.cpp
    settings.cpp
    interface.cpp
    xwlshell.cpp
    wl_shell/wlshell.cpp
    wl_shell/wlshellsurface.cpp
    effects/scaleeffect.cpp
    effects/griddesktops.cpp
    effects/zoomeffect.cpp
    effects/fademovingeffect.cpp
    effects/inoutsurfaceeffect.cpp
    effects/minimizeeffect.cpp)

wayland_add_protocol_server(SOURCES
    ${CMAKE_SOURCE_DIR}/protocol/desktop-shell.xml
    desktop-shell
)

wayland_add_protocol_server(SOURCES
    ${CMAKE_SOURCE_DIR}/protocol/input-method.xml
    input-method
)

add_library(nuclear-shell-common SHARED ${SOURCES})
set_target_properties(nuclear-shell-common PROPERTIES COMPILE_DEFINITIONS WL_HIDE_DEPRECATED=1)

set(DESKTOP
    desktop_shell/desktopshellwindow.cpp
    desktop_shell/desktopshellworkspace.cpp
    desktop_shell/desktop-shell.cpp)

add_library(nuclear-desktop-shell SHARED ${DESKTOP})
set_target_properties(nuclear-desktop-shell PROPERTIES PREFIX "")
set_target_properties(nuclear-desktop-shell PROPERTIES COMPILE_DEFINITIONS WL_HIDE_DEPRECATED=1)
target_link_libraries(nuclear-desktop-shell nuclear-shell-common)

install(TARGETS nuclear-shell-common DESTINATION lib/nuclear-shell)
install(TARGETS nuclear-desktop-shell DESTINATION lib/nuclear-shell)
