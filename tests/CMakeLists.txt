find_package(GTest QUIET)

if(GTest_FOUND)
    add_executable(test_runner
        test_tcp.cc
        test_main.cc
    )
    
    target_link_libraries(test_runner
        PRIVATE
        ridics_lib
        GTest::gtest
        GTest::gtest_main
    )
    
    target_compile_definitions(test_runner
        PRIVATE
        ${PQXX_CFLAGS_OTHER}
    )
    
    include(GoogleTest)
    gtest_discover_tests(test_runner)
    
    message(STATUS "Google Test found. Tests enabled.")
else()
    message(WARNING "Google Test not found. Tests disabled.")
    
    add_custom_target(test_runner
        COMMAND ${CMAKE_COMMAND} -E echo "Tests not available - Google Test not found"
    )
endif()