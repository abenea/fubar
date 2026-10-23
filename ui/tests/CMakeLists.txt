add_executable(playlistmodeltest playlistmodeltest.cpp)
target_link_libraries(playlistmodeltest
    PRIVATE ui player navidrome library GTest::gtest GTest::gmock Qt::Test)
gtest_discover_tests(playlistmodeltest)

add_executable(navidromeuitest
    navidromeuitest.cpp
    ${PROJECT_SOURCE_DIR}/player/tests/mockaudiooutput.cpp)
target_link_libraries(navidromeuitest
    PRIVATE ui player navidrome library util fakesubsonicserver GTest::gtest Qt::Test)
gtest_discover_tests(navidromeuitest)
