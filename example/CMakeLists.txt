# COMPILE THE EXAMPLE CODE (example/example.cpp)
add_executable(example example.cpp)
set_target_properties(example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(example Threads::Threads)

add_executable(range_example range_example.cpp)
set_target_properties(range_example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(range_example Threads::Threads)
