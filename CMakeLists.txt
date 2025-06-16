cmake_minimum_required(VERSION 3.28...3.31)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# Enable C++17 for all platforms
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find OBS and dependencies
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Find libwebsockets and nlohmann_json
find_package(nlohmann_json REQUIRED)

if(OS_WINDOWS)
  # On Windows, look for libwebsockets in common locations
  find_path(
    LIBWEBSOCKETS_INCLUDE_DIR
    NAMES libwebsockets.h
    PATHS
      $ENV{LIBWEBSOCKETS_ROOT}/include
      $ENV{LIBWEBSOCKETS_ROOT}/build
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/include
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build
      C:/libwebsockets/include
  )

  find_library(
    LIBWEBSOCKETS_LIBRARY
    NAMES websockets_static websockets libwebsockets
    PATHS 
      $ENV{LIBWEBSOCKETS_ROOT}/build/lib/Release
      $ENV{LIBWEBSOCKETS_ROOT}/build/Release
      $ENV{LIBWEBSOCKETS_ROOT}/build/lib
      $ENV{LIBWEBSOCKETS_ROOT}/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build/lib/Release
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build/Release
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/lib
      C:/libwebsockets/lib
    PATH_SUFFIXES Release Debug
  )
elseif(OS_MACOS)
  # On macOS, libwebsockets should be built by Install-MacOSDeps.sh
  # Check environment variable first
  if(DEFINED ENV{LIBWEBSOCKETS_ROOT})
    set(LIBWEBSOCKETS_ROOT "$ENV{LIBWEBSOCKETS_ROOT}")
  else()
    set(LIBWEBSOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets")
  endif()
  
  find_path(
    LIBWEBSOCKETS_INCLUDE_DIR
    NAMES libwebsockets.h
    PATHS
      ${LIBWEBSOCKETS_ROOT}/include
      /opt/homebrew/include
      /usr/local/include
  )
  
  find_library(
    LIBWEBSOCKETS_LIBRARY
    NAMES websockets libwebsockets
    PATHS
      ${LIBWEBSOCKETS_ROOT}/build/lib
      ${LIBWEBSOCKETS_ROOT}/lib
      /opt/homebrew/lib
      /usr/local/lib
  )
elseif(OS_LINUX)
  # On Linux, use pkg-config if available
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBWEBSOCKETS libwebsockets)
  endif()

  find_path(
    LIBWEBSOCKETS_INCLUDE_DIR
    NAMES libwebsockets.h
    HINTS ${PC_LIBWEBSOCKETS_INCLUDE_DIRS}
    PATHS /usr/include /usr/local/include
  )

  find_library(
    LIBWEBSOCKETS_LIBRARY
    NAMES websockets
    HINTS ${PC_LIBWEBSOCKETS_LIBRARY_DIRS}
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
  )
endif()

if(NOT LIBWEBSOCKETS_INCLUDE_DIR OR NOT LIBWEBSOCKETS_LIBRARY)
  message(STATUS "libwebsockets not found")
  message(STATUS "Include dir: ${LIBWEBSOCKETS_INCLUDE_DIR}")
  message(STATUS "Library: ${LIBWEBSOCKETS_LIBRARY}")
  if(OS_WINDOWS)
    message(STATUS "On Windows, make sure libwebsockets is installed and LIBWEBSOCKETS_ROOT is set")
  elseif(OS_LINUX)
    message(STATUS "On Linux, install libwebsockets-dev via apt")
  elseif(OS_MACOS)
    message(STATUS "On macOS, install libwebsockets via Homebrew")
  endif()
  message(FATAL_ERROR "libwebsockets not found. Please install libwebsockets")
else()
  message(STATUS "Found libwebsockets:")
  message(STATUS "  Include: ${LIBWEBSOCKETS_INCLUDE_DIR}")
  message(STATUS "  Library: ${LIBWEBSOCKETS_LIBRARY}")
endif()

# Plugin files
set(plugin_SOURCES src/plugin-main.cpp src/audio-streamer.cpp src/websocket-client.cpp src/settings-dialog.cpp)

set(
  plugin_HEADERS
  include/obs-audio-to-websocket/audio-streamer.hpp
  include/obs-audio-to-websocket/websocket-client.hpp
  include/obs-audio-to-websocket/settings-dialog.hpp
  include/obs-audio-to-websocket/audio-format.hpp
)

# UI files
set(plugin_UI ui/settings-dialog.ui)

# Add plugin
add_library(${CMAKE_PROJECT_NAME} MODULE ${plugin_SOURCES} ${plugin_HEADERS})

# Plugin includes
target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${LIBWEBSOCKETS_INCLUDE_DIR}
)

# For Windows, also include the build directory for lws_config.h
if(OS_WINDOWS)
  # First try the environment variable path
  if(DEFINED ENV{LIBWEBSOCKETS_ROOT})
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE $ENV{LIBWEBSOCKETS_ROOT}/build)
  endif()
  # Also try the local deps path
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build")
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/libwebsockets/build)
  endif()
endif()

# Platform-specific definitions
if(OS_WINDOWS)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0603)
endif()

# Link libraries
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets nlohmann_json::nlohmann_json ${LIBWEBSOCKETS_LIBRARY}
)

# libwebsockets compatibility fix
if(OS_LINUX)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -fpermissive)
elseif(OS_MACOS)
  # Disable the specific warning that's being treated as error
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wno-error=missing-declarations)
elseif(OS_WINDOWS)
  # For Windows, define macros to exclude problematic features
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE 
    LWS_WITH_STRUCT_JSON=0
    LWS_WITH_STRUCT_SQLITE3=0
    _CRT_SECURE_NO_WARNINGS
  )
endif()

# Windows specific
if(OS_WINDOWS)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ws2_32)

  if(NOT DEFINED OBS_RUNTIME_DIR)
    set(OBS_RUNTIME_DIR "${OBS_DIR}/bin/64bit")
  endif()

  set(OBS_LIBRARY_DESTINATION "${OBS_RUNTIME_DIR}")
  set(OBS_PLUGIN_DESTINATION "obs-plugins/64bit")
endif()

# macOS specific
if(OS_MACOS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Linux specific
if(OS_LINUX)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE pthread)
endif()

# Set Qt properties
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

# UI search path
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Plugin configuration
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})
