set(playertest_SRC
    mockaudiooutput.cpp
    playertest.cpp)
set(lib_DEPS player library ui Qt5::Test)
add_qtest(playertest playertest_SRC lib_DEPS)
