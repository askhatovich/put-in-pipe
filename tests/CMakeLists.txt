include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Helper function to add a test
function(add_pip_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE put-in-pipe-core gtest_main)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Unit tests
add_pip_test(test_atomicset test_atomicset.cpp)
add_pip_test(test_buffer test_buffer.cpp)
add_pip_test(test_chunk test_chunk.cpp)
add_pip_test(test_timercallback test_timercallback.cpp)
add_pip_test(test_observer test_observer.cpp)
add_pip_test(test_client test_client.cpp)
add_pip_test(test_serializable_event test_serializable_event.cpp)
add_pip_test(test_config test_config.cpp)

# Integration tests
add_pip_test(test_integration_transfer test_integration_transfer.cpp)
