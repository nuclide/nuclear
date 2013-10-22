

pkg_check_modules(WaylandServer wayland-server REQUIRED)
pkg_check_modules(Pixman pixman-1 REQUIRED)
pkg_check_modules(Weston weston REQUIRED)

include_directories(
    ${WaylandServer_INCLUDE_DIRS}
    ${Pixman_INCLUDE_DIRS}
    ${Weston_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    ${CMAKE_BINARY_DIR}/protocol
)

set(SOURCES
    shell.cpp
    layer.cpp
    shellsurface.cpp
    shellseat.cpp
    workspace.cpp
    effect.cpp
    transform.cpp
    animation.cpp
    desktop-shell.cpp
    inputpanel.cpp
    effects/scaleeffect.cpp
    effects/griddesktops.cpp
    effects/zoomeffect.cpp
    effects/fademovingeffect.cpp
    effects/inoutsurfaceeffect.cpp
    effects/minimizeeffect.cpp)

add_library(nuclear-shell SHARED ${SOURCES})
set_target_properties(nuclear-shell PROPERTIES PREFIX "")
set_target_properties(nuclear-shell PROPERTIES COMPILE_DEFINITIONS WL_HIDE_DEPRECATED=1)

install(TARGETS nuclear-shell DESTINATION lib/nuclear-shell)
