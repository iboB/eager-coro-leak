cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(eager-coro-leak
    LANGUAGES CXX
)

# standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# sanitizers
option(SAN_ADDR "${CMAKE_PROJECT_NAME}: sanitize address" OFF)
option(SAN_UB "${CMAKE_PROJECT_NAME}: sanitize undefined behavior" OFF)
option(SAN_LEAK "${CMAKE_PROJECT_NAME}: sanitize leaks" OFF)

if(MSVC)
    if(SAN_ADDR)
        add_compile_options(-fsanitize=address)
    endif()
    if(SAN_UB OR SAN_LEAK)
        message(WARNING "Unsupported sanitizers requested for msvc. Ignored")
    endif()
else()
    if(SAN_ADDR)
        list(APPEND sanitizerFlags -fsanitize=address -pthread)
    endif()
    if(SAN_UB)
        list(APPEND sanitizerFlags -fsanitize=undefined)
    endif()
    if(SAN_LEAK)
        if(APPLE)
            message(WARNING "Unsupported leak sanitizer requested for Apple. Ignored")
        else()
            list(APPEND sanitizerFlags -fsanitize=leak)
        endif()
    endif()
    if(sanitizerFlags)
        add_compile_options(${sanitizerFlags})
        add_link_options(${sanitizerFlags})
    endif()
endif()

# target
add_executable(main main.cpp)
