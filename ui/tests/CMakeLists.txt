set(lib_DEPS ui library ${QT_LIBRARIES} ${PROTOBUF_LIBRARY} ${TAGLIB_LIBRARY})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(filtertest_SRC FilterTest.cpp)
add_qtest(filtertest filtertest_SRC lib_DEPS)