protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS track.proto)

add_library(library
            directorywatcher.cpp
            directory.cpp
            track.cpp
            cuefile.cpp
            library.cpp
            ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(library PRIVATE util Qt::Core protobuf::libprotobuf Cue Taglib)
