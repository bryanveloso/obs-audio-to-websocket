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

# Find dependencies
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig)

# Find libwebsockets
if(OS_WINDOWS)
  # Windows: User must provide libwebsockets via CMAKE_PREFIX_PATH
  find_package(websockets QUIET)
  if(NOT websockets_FOUND)
    find_path(LIBWEBSOCKETS_INCLUDE_DIR
      NAMES libwebsockets.h
    )
    find_library(LIBWEBSOCKETS_LIBRARY
      NAMES websockets_static websockets
    )
  else()
    set(LIBWEBSOCKETS_INCLUDE_DIR ${websockets_INCLUDE_DIRS})
    set(LIBWEBSOCKETS_LIBRARY ${websockets_LIBRARIES})
  endif()
else()
  # macOS/Linux: Use system libraries
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBWEBSOCKETS libwebsockets)
  endif()

  find_path(LIBWEBSOCKETS_INCLUDE_DIR
    NAMES libwebsockets.h
    HINTS ${LIBWEBSOCKETS_INCLUDE_DIRS}
    PATHS
      /usr/include
      /usr/local/include
      /opt/homebrew/include
  )

  find_library(LIBWEBSOCKETS_LIBRARY
    NAMES websockets
    HINTS ${LIBWEBSOCKETS_LIBRARY_DIRS}
    PATHS
      /usr/lib
      /usr/local/lib
      /usr/lib/x86_64-linux-gnu
      /opt/homebrew/lib
  )
endif()

if(NOT LIBWEBSOCKETS_INCLUDE_DIR OR NOT LIBWEBSOCKETS_LIBRARY)
  if(OS_WINDOWS)
    message(FATAL_ERROR "libwebsockets not found. Please install libwebsockets and add it to CMAKE_PREFIX_PATH.")
  else()
    message(FATAL_ERROR "libwebsockets not found. Please install libwebsockets.")
  endif()
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
