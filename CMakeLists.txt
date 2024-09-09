cmake_minimum_required(VERSION 3.20)
project(MollyBet)

# Policies
#--------------------------------------
cmake_policy(SET CMP0167 NEW)

# C++ version
#--------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#--------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Prefer static libs
#--------------------------------------
set(BUILD_SHARED_LIBS OFF)

# VCPKG
#--------------------------------------
if(NOT DEFINED ENV{VCPKG_FEATURE_FLAGS})
    set(ENV{VCPKG_FEATURE_FLAGS} "binarycaching")
endif()

include(${CMAKE_SOURCE_DIR}/dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(VCPKG_MANIFEST_MODE ON)

# Dependencies
#--------------------------------------
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS beast)
find_package(OpenSSL REQUIRED)

# Set output directory
#--------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

# Executable
#--------------------------------------
set(SRC_FILES
    src/main.cpp
    src/session.cpp
    src/session.h

    src/log/logger.cpp
    src/log/logger.h
    src/log/loggerColorConsole.h
    src/log/loggerConsole.h
    src/log/loggerFile.h
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SRC_FILES})

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
)

if(MSVC OR MINGW)
    # Limit to Windows 7
    target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        _WIN32_WINDOWS=0x0601
        _WIN32_WINNT=0x0601
    )
endif()

target_include_directories(${PROJECT_NAME}
PRIVATE
    src
)

if (MSVC)
    target_compile_options(${PROJECT_NAME}
    PRIVATE
        /bigobj
    )
elseif (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--stack,16777216)
        target_compile_options(${PROJECT_NAME} PRIVATE -Wa,-mbig-obj)
endif()

target_link_libraries(${PROJECT_NAME}
PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::beast
    fmt::fmt-header-only
)

if(MSVC OR MINGW)
    target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ws2_32
    )
endif()

if(NOT MSVC)
    target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall -Wextra
    )
endif()
