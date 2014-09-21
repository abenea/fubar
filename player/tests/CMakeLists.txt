include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(playertest_SRC
    mockaudiooutput.cpp
    playertest.cpp)
set(lib_DEPS player library ui ${QT_LIBRARIES} ${KDE4_KDEUI_LIBS})
add_qtest(playertest playertest_SRC lib_DEPS)
