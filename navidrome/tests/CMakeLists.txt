add_library(fakesubsonicserver STATIC fakesubsonicserver.cpp)
target_link_libraries(fakesubsonicserver PUBLIC Qt::Core Qt::Network)

add_executable(navidrometest navidrometest.cpp)
target_link_libraries(navidrometest
    PRIVATE navidrome fakesubsonicserver library util protobuf::libprotobuf GTest::gtest
            GTest::gmock Qt::Test)
gtest_discover_tests(navidrometest)

add_executable(navidromelivetest navidromelivetest.cpp)
target_link_libraries(navidromelivetest
    PRIVATE navidrome library GTest::gtest Qt::Network Qt::Test)
gtest_discover_tests(navidromelivetest)
