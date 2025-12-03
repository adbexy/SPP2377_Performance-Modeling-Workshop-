cmake_minimum_required(VERSION 3.18)

# set the project name
project(vmanager VERSION 0.1)

# specify the C standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DEBUG_FLAGS "-g3" "-ggdb" "-save-temps" "-fno-omit-frame-pointer" "-O0")
set(RELEASE_FLAGS "-O3")

#set flags used for Release and Debug build type
add_compile_options(
    "$<$<CONFIG:Release>:${RELEASE_FLAGS}>"
    "$<$<CONFIG:Debug>:${DEBUG_FLAGS}>"
)

# build directory
set(CMAKE_BINARY_DIR "bin") #relative to inside build
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})


# include directories
add_subdirectory(modules/SIMDOps modules/SIMDOps/bin)

# link libraries
link_libraries(-lnuma -lpthread)

add_library(threadman STATIC
    code/utils/threads/ThreadManager.cpp
    code/utils/threads/ThreadGroup.cpp
    code/utils/threads/ThreadWrapper.cpp
)
target_include_directories(threadman PUBLIC code/utils)
target_link_libraries(threadman PUBLIC pthread numa)

option(TESTING "enable testing mode" ON)

set(SIMDOPS_CXX_STANDARD 20)
create_target(
    TARGET_NAME simdops_filter
    OUT_PATH ${CMAKE_BINARY_DIR}
    SRC_FILES code/filter.cpp
    INC_DIRECTORIES
        modules/SIMDOps/include
        code/utils
    LIBRARIES
    COMPILE_DEFS
    COMPILE_OPTS
    LINK_OPTS
)

set(SIMDOPS_CXX_STANDARD 20)
create_target(
        TARGET_NAME simdops_query
        OUT_PATH ${CMAKE_BINARY_DIR}
        SRC_FILES code/query.cpp code/utils/threads/ThreadManager.cpp code/utils/threads/ThreadWrapper.cpp code/utils/threads/ThreadGroup.cpp
        INC_DIRECTORIES
        modules/SIMDOps/include
        code/utils
        LIBRARIES
        COMPILE_DEFS
        TESTING=$<BOOL:${TESTING}>
        COMPILE_OPTS
        LINK_OPTS
)

#add_executable(vam_alloc_test test/vam_alloc_test.cpp)

