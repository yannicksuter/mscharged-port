include_guard(GLOBAL)
add_library(charged_events STATIC
    "${MSCHARGED_PREPARED}/src/NL/nlEvent.cpp"
    "${MSCHARGED_PREPARED}/src/NL/TaskBase.cpp"
    "${MSCHARGED_PREPARED}/src/Game/Task/DispatchEventsTask.cpp"
    src/runtime/events.cpp src/runtime/event_queue.cpp src/runtime/event_task.cpp)
add_dependencies(charged_events verify_prepared)
target_include_directories(charged_events PUBLIC src
    "${MSCHARGED_PREPARED}/libs/RVL_SDK/include")
target_link_libraries(charged_events PUBLIC charged_graphics_memory)
target_compile_features(charged_events PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
    target_compile_options(charged_events PRIVATE -ffp-contract=off -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas)
endif()
if(BUILD_TESTING)
    add_executable(runtime_events_tests tests/runtime_events.cpp)
    target_link_libraries(runtime_events_tests PRIVATE charged_events)
    add_test(NAME runtime_events COMMAND runtime_events_tests)
    set_tests_properties(runtime_events PROPERTIES TIMEOUT 30)
    add_executable(queued_events_tests tests/queued_events.cpp)
    target_link_libraries(queued_events_tests PRIVATE charged_events)
    add_test(NAME queued_events COMMAND queued_events_tests)
    set_tests_properties(queued_events PROPERTIES TIMEOUT 30)
endif()
