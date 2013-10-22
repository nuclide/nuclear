
set(SOURCES)

wayland_add_protocol_server(SOURCES ${CMAKE_SOURCE_DIR}/protocol/desktop-shell.xml nuclear-desktop-shell)
wayland_add_protocol_server(SOURCES ${CMAKE_SOURCE_DIR}/protocol/input-method.xml weston-input-method)

wayland_add_protocol_client(SOURCES ${CMAKE_SOURCE_DIR}/protocol/desktop-shell.xml nuclear-desktop-shell)

add_custom_target(protocol ALL SOURCES ${SOURCES})

install(FILES desktop-shell.xml DESTINATION share/nuclear-shell RENAME nuclear-desktop-shell.xml)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nuclear-desktop-shell-client-protocol.h DESTINATION include/nuclear-shell)
