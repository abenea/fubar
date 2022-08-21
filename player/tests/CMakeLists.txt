add_executable(playertest mockaudiooutput.cpp playertest.cpp)
target_link_libraries(playertest PRIVATE player library ui GTest::gtest)
gtest_discover_tests(playertest)
