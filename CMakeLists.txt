cmake_minimum_required(VERSION 3.14)
project(origo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опции для Debug и Release
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")

include(FetchContent)

# llhttp
FetchContent_Declare(
    llhttp
    GIT_REPOSITORY https://github.com/nodejs/llhttp.git
    GIT_TAG release/v9.2.1
)

# ASIO
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0
)

FetchContent_Declare(
    expected
    GIT_REPOSITORY https://github.com/martinmoene/expected-lite.git
    GIT_TAG v0.7.0
)

# 4. optional (тоже может понадобиться)
FetchContent_Declare(
    optional
    GIT_REPOSITORY https://github.com/martinmoene/optional-lite.git
    GIT_TAG v3.6.0
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)

# RESTinio
FetchContent_Declare(
    restinio
    GIT_REPOSITORY https://github.com/Stiffstream/restinio.git
    GIT_TAG v.0.7.0
)

# Загружаем зависимости llhttp
FetchContent_GetProperties(llhttp)
if(NOT llhttp_POPULATED)
    FetchContent_Populate(llhttp)
    
    # Собираем llhttp со всеми源 файлами
    add_library(llhttp STATIC
        ${llhttp_SOURCE_DIR}/src/llhttp.c
        ${llhttp_SOURCE_DIR}/src/api.c
        ${llhttp_SOURCE_DIR}/src/http.c
    )
    target_include_directories(llhttp PUBLIC
        ${llhttp_SOURCE_DIR}/include
    )
    target_compile_definitions(llhttp PRIVATE 
        LLHTTP__WIN32
    )
endif()

# Загружаем зависимости
FetchContent_MakeAvailable(asio expected optional fmt restinio)

# Собираем fmt статически
if(NOT TARGET fmt)
    add_library(fmt STATIC)
    target_sources(fmt PRIVATE
        ${fmt_SOURCE_DIR}/src/format.cc
        ${fmt_SOURCE_DIR}/src/os.cc
    )
    target_include_directories(fmt PUBLIC
        ${fmt_SOURCE_DIR}/include
    )
endif()

# Создаем библиотеку с зависимостями RESTinio
add_library(restinio_deps INTERFACE)
target_include_directories(restinio_deps INTERFACE
    ${restinio_SOURCE_DIR}/dev
    ${asio_SOURCE_DIR}/asio/include
    ${expected_SOURCE_DIR}/include
    ${optional_SOURCE_DIR}/include
    ${fmt_SOURCE_DIR}/include
    ${llhttp_SOURCE_DIR}/include
)
target_link_libraries(restinio_deps INTERFACE llhttp fmt pthread)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE restinio_deps)

message(STATUS "✅ RESTinio configured")