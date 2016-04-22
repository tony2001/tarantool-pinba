# Build
if (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined suppress -flat_namespace")
endif(APPLE)

add_library(pinba SHARED lib.c pinba.pb-c.c protobuf-c.c)
set_target_properties(pinba
        PROPERTIES
            PREFIX ""
            SUFFIX ".so"
            OUTPUT_NAME "lib")

# Install
install(TARGETS pinba LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/pinba)
install(FILES client.lua DESTINATION ${TARANTOOL_INSTALL_LUADIR}/pinba)
