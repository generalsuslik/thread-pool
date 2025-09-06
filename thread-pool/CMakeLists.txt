set(TARGET executor)

set(SOURCES
        executor.cpp
        task.cpp
)

add_library(executor
        ${SOURCES}
)

target_include_directories(${TARGET}
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)