add_executable(playlistmodeltest playlistmodeltest.cpp)
target_link_libraries(playlistmodeltest PRIVATE ui GTest::gtest GTest::gmock Qt::Test)
gtest_discover_tests(playlistmodeltest)
