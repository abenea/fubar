set(lib_DEPS ui library ${QT_LIBRARIES} ${PROTOBUF_LIBRARY} ${TAGLIB_LIBRARY} ${KDE4_KDEUI_LIBS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(filtertest_SRC FilterTest.cpp)
set(playertest_SRC mockaudiooutput.cpp ../../audiooutput.cpp ../../util.cpp ../../plugins/pluginmanager.cpp playertest.cpp)
add_qtest(filtertest filtertest_SRC lib_DEPS)
add_qtest(playertest playertest_SRC lib_DEPS)