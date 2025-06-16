cmake_minimum_required(VERSION 3.16...3.26)

project(obs-audio-to-websocket VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Add OBS cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find libobs
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

# Find Boost for WebSocket++
find_package(Boost REQUIRED COMPONENTS system thread)

# Find nlohmann/json
find_package(nlohmann_json QUIET)

# Plugin files
set(obs-audio-to-websocket_SOURCES
    src/plugin-main.cpp
    src/audio-streamer.cpp
    src/websocket-client.cpp
    src/settings-dialog.cpp
)

set(obs-audio-to-websocket_HEADERS
    include/obs-audio-to-websocket/audio-streamer.hpp
    include/obs-audio-to-websocket/websocket-client.hpp
    include/obs-audio-to-websocket/settings-dialog.hpp
    include/obs-audio-to-websocket/audio-format.hpp
)

# UI files
set(obs-audio-to-websocket_UI
    ui/settings-dialog.ui
)

# Create plugin
add_library(obs-audio-to-websocket MODULE
    ${obs-audio-to-websocket_SOURCES}
    ${obs-audio-to-websocket_HEADERS}
)

# Qt MOC
qt6_wrap_cpp(obs-audio-to-websocket_MOC ${obs-audio-to-websocket_HEADERS})
qt6_wrap_ui(obs-audio-to-websocket_UI_HEADERS ${obs-audio-to-websocket_UI})

target_sources(obs-audio-to-websocket PRIVATE
    ${obs-audio-to-websocket_MOC}
    ${obs-audio-to-websocket_UI_HEADERS}
)

# Include directories
target_include_directories(obs-audio-to-websocket PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Find WebSocket++ in various locations
find_path(WEBSOCKETPP_INCLUDE_DIR
    NAMES websocketpp/config/asio_no_tls.hpp
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp
        ${WEBSOCKETPP_ROOT}
        /usr/include
        /usr/local/include
        /opt/homebrew/include
)

if(WEBSOCKETPP_INCLUDE_DIR)
    target_include_directories(obs-audio-to-websocket PRIVATE ${WEBSOCKETPP_INCLUDE_DIR})
else()
    message(FATAL_ERROR "WebSocket++ not found. Please install it or set WEBSOCKETPP_ROOT")
endif()

# Find nlohmann/json in various locations
find_path(JSON_INCLUDE_DIR
    NAMES nlohmann/json.hpp
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/json/include
        /usr/include
        /usr/local/include
        /opt/homebrew/include
)

if(JSON_INCLUDE_DIR)
    target_include_directories(obs-audio-to-websocket PRIVATE ${JSON_INCLUDE_DIR})
elseif(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann/json not found. Please install it")
endif()

# Link libraries
target_link_libraries(obs-audio-to-websocket PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Core
    Qt6::Widgets
    Boost::system
    Boost::thread
)

if(nlohmann_json_FOUND)
    target_link_libraries(obs-audio-to-websocket PRIVATE nlohmann_json::nlohmann_json)
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(obs-audio-to-websocket PRIVATE _WEBSOCKETPP_CPP11_STL_)
    target_link_libraries(obs-audio-to-websocket PRIVATE ws2_32)
endif()

if(APPLE)
    set_target_properties(obs-audio-to-websocket PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/Info.plist.in"
    )
endif()

# Install
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_bit_suffix "64bit")
else()
    set(_bit_suffix "32bit")
endif()

set_target_properties(obs-audio-to-websocket PROPERTIES PREFIX "")

install(TARGETS obs-audio-to-websocket
    LIBRARY DESTINATION "obs-plugins/${_bit_suffix}"
    RUNTIME DESTINATION "obs-plugins/${_bit_suffix}"
)

install(DIRECTORY data/
    DESTINATION "data/obs-plugins/obs-audio-to-websocket"
)