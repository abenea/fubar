add_executable(playertest mockaudiooutput.cpp playertest.cpp)
target_link_libraries(playertest PRIVATE player library ui GTest::gtest)
gtest_discover_tests(playertest)

add_executable(jsonprocesstest jsonprocesstest.cpp)
target_link_libraries(jsonprocesstest PRIVATE player GTest::gtest_main)
gtest_discover_tests(jsonprocesstest)
