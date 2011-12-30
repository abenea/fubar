wrap_proto(PROTO_GENERATED track.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${TAGLIB_INCLUDES})
add_library(library
			directorywatcher.cpp
            directory.cpp
            track.cpp
            library.cpp
            playlist.cpp
            ${PROTO_GENERATED})
target_link_libraries(library ${PROTOBUF_LIBRARY} ${TAGLIB_LIBRARIES})
