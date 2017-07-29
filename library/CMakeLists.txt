protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS track.proto)
include_directories(${TAGLIB_INCLUDE_DIR} ${CUE_INCLUDE_DIRS})
add_library(library
            directorywatcher.cpp
            directory.cpp
            track.cpp
            cuefile.cpp
            library.cpp
            ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(library Qt5::Core ${PROTOBUF_LIBRARY} ${TAGLIB_LIBRARY} ${CUE_LIBRARIES})
