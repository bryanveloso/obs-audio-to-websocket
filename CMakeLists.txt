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

# Try to find WebSocket++ and Asio like obs-websocket does
find_package(Websocketpp 0.8 QUIET)
find_package(Asio 1.12.1 QUIET)

# If not found, use FetchContent as fallback
if(NOT Websocketpp_FOUND OR NOT Asio_FOUND)
  include(FetchContent)

  if(NOT Asio_FOUND)
    FetchContent_Declare(asio GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git GIT_TAG asio-1-12-1)
  endif()

  if(NOT Websocketpp_FOUND)
    FetchContent_Declare(websocketpp GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git GIT_TAG 0.8.2)
  endif()

  # Handle Asio first
  if(NOT Asio_FOUND)
    FetchContent_MakeAvailable(asio)
  endif()

  # Handle WebSocket++ with patching
  if(NOT Websocketpp_FOUND)
    FetchContent_GetProperties(websocketpp)
    if(NOT websocketpp_POPULATED)
      # Allow deprecated FetchContent_Populate for manual patching
      cmake_policy(PUSH)
      cmake_policy(SET CMP0169 OLD)
      FetchContent_Populate(websocketpp)
      cmake_policy(POP)

      # Patch the CMake minimum version
      file(READ ${websocketpp_SOURCE_DIR}/CMakeLists.txt WEBSOCKETPP_CMAKE)
      string(
        REPLACE
        "cmake_minimum_required (VERSION 2.6)"
        "cmake_minimum_required (VERSION 3.5)"
        WEBSOCKETPP_CMAKE
        "${WEBSOCKETPP_CMAKE}"
      )
      file(WRITE ${websocketpp_SOURCE_DIR}/CMakeLists.txt "${WEBSOCKETPP_CMAKE}")

      # WebSocket++ is header-only, we don't need to build or install anything
      # Just make the headers available
      set(websocketpp_POPULATED TRUE CACHE BOOL "WebSocket++ populated" FORCE)
    endif()
  endif()
endif()

# Plugin files
set(
  plugin_SOURCES
  src/plugin-main.cpp
  src/audio-streamer.cpp
  src/websocketpp-client.cpp
  src/settings-dialog.cpp
  src/audio-format.cpp
)

set(
  plugin_HEADERS
  include/obs-audio-to-websocket/audio-streamer.hpp
  include/obs-audio-to-websocket/websocketpp-client.hpp
  include/obs-audio-to-websocket/settings-dialog.hpp
  include/obs-audio-to-websocket/audio-format.hpp
  include/obs-audio-to-websocket/obs-source-wrapper.hpp
  include/obs-audio-to-websocket/constants.hpp
)

# UI files (currently none - UI is created programmatically)
# set(plugin_UI)

# Add plugin
add_library(${CMAKE_PROJECT_NAME} MODULE ${plugin_SOURCES} ${plugin_HEADERS})

# Plugin includes
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add include directories based on how deps were found
if(Websocketpp_FOUND)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Websocketpp::Websocketpp)
else()
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${websocketpp_SOURCE_DIR})
endif()

if(Asio_FOUND)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Asio::Asio)
else()
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${asio_SOURCE_DIR}/asio/include)
endif()

# Add OBS dependency includes for SSL support on Windows
if(OS_WINDOWS)
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_PREFIX_PATH}/include)
endif()

# Link libraries
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets nlohmann_json::nlohmann_json
)

# WebSocket++ requires these definitions (matching obs-websocket)
target_compile_definitions(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    ASIO_STANDALONE # Use standalone Asio, not Boost.Asio
    $<$<PLATFORM_ID:Windows>:_WEBSOCKETPP_CPP11_STL_> # Only on Windows like obs-websocket
)

# Platform-specific settings
if(OS_LINUX)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -fpermissive -Wno-error=format-overflow)
elseif(OS_MACOS)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE -Wno-error=missing-declarations -Wno-error=deprecated-declarations
  )
elseif(OS_WINDOWS)
  target_compile_definitions(
    ${CMAKE_PROJECT_NAME}
    PRIVATE _WIN32_WINNT=0x0603 _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX
  )
  # Ensure MSVC properly detects C++ mode and disable specific warnings from WebSocket++
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /Zc:__cplusplus /wd4267)
endif()

# Windows specific
if(OS_WINDOWS)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ws2_32 mswsock)

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

# Install locale files
install(DIRECTORY data/locale DESTINATION data/obs-plugins/${CMAKE_PROJECT_NAME} FILES_MATCHING PATTERN "*.ini")
