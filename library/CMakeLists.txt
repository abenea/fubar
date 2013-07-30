wrap_proto(PROTO_GENERATED track.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR} ${TAGLIB_INCLUDE_DIR} ${CUE_INCLUDE_DIR})
qt4_automoc(library.cpp)
add_library(library
            directorywatcher.cpp
            directory.cpp
            track.cpp
            cuefile.cpp
            library.cpp
            ${PROTO_GENERATED})
target_link_libraries(library ${PROTOBUF_LIBRARY} ${TAGLIB_LIBRARY} ${CUE_LIBRARIES})
