# These are the public headers that get installed
# No internal headers should go in this variable
set(headers
    lcc.h
    lcc-print.h
    lcc-common.h
    lcc-gridconnect.h
    simplelogger_defs.h
)

add_library(lcc STATIC
    ${headers}
    lcc-simple.h
    lcc-common-internal.h
    lcc-common-internal.c
    lcc-addressed.h
    lcc-addressed.c
    lcc.c
    lcc-print.c
    lcc-gridconnect.c
    lcc-simple.c
    lcc-network-info.h
    lcc-network-info.c
    lcc-node-info.h
    lcc-node-info.c
    lcc-network-internal.h
    lcc-datagram.c
    lcc-datagram.h
    lcc-memory.c
    lcc-memory.h
    lcc-event.c
    lcc-event.h
    lcc-simple-node-info.c
    lcc-simple-node-info.h
    lcc-remote-memory.c
    lcc-remote-memory.h
)
target_compile_definitions(lcc PRIVATE
    LIBLCC_BUILD
    LIBLCC_MAJOR=0
    LIBLCC_MINOR=5
    LIBLCC_MICRO=0
    ARDUINO
)
target_include_directories( lcc INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include/lcc>
)
# https://devzone.nordicsemi.com/f/nordic-q-a/80417/compile-part-of-application-as-static-library
target_link_libraries(lcc PUBLIC zephyr_interface)
