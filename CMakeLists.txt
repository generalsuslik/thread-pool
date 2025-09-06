cmake_minimum_required(VERSION 3.28)
project(thread_pool)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(thread-pool)

add_executable(thread_pool main.cpp)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        executor
)