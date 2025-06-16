# Find obs-frontend-api
#
# This module defines
#  OBS_FRONTEND_API_FOUND
#  OBS_FRONTEND_API_INCLUDE_DIRS
#  OBS_FRONTEND_API_LIBRARIES

find_path(OBS_FRONTEND_API_INCLUDE_DIR
    NAMES obs-frontend-api.h
    PATHS
        ENV OBS_DIR
        ${OBS_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/include
        /usr/include
        /opt/local/include
        /opt/homebrew/include
    PATH_SUFFIXES
        include
        include/obs
        obs
        obs-frontend-api
        UI/obs-frontend-api
)

find_library(OBS_FRONTEND_API_LIBRARY
    NAMES obs-frontend-api libobs-frontend-api
    PATHS
        ENV OBS_DIR
        ${OBS_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /opt/local/lib
        /opt/homebrew/lib
    PATH_SUFFIXES
        lib
        lib${LIB_SUFFIX}
        bin
        bin${LIB_SUFFIX}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(obs-frontend-api DEFAULT_MSG OBS_FRONTEND_API_LIBRARY OBS_FRONTEND_API_INCLUDE_DIR)

if(OBS_FRONTEND_API_FOUND)
    set(OBS_FRONTEND_API_INCLUDE_DIRS ${OBS_FRONTEND_API_INCLUDE_DIR})
    set(OBS_FRONTEND_API_LIBRARIES ${OBS_FRONTEND_API_LIBRARY})
    
    if(NOT TARGET OBS::obs-frontend-api)
        add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
        set_target_properties(OBS::obs-frontend-api PROPERTIES
            IMPORTED_LOCATION "${OBS_FRONTEND_API_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OBS_FRONTEND_API_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(OBS_FRONTEND_API_INCLUDE_DIR OBS_FRONTEND_API_LIBRARY)