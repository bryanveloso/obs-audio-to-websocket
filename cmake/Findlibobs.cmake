# Find libobs
#
# This module defines
#  LIBOBS_FOUND
#  LIBOBS_INCLUDE_DIRS
#  LIBOBS_LIBRARIES

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
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
        libobs
)

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
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
find_package_handle_standard_args(libobs DEFAULT_MSG LIBOBS_LIBRARY LIBOBS_INCLUDE_DIR)

if(LIBOBS_FOUND)
    set(LIBOBS_INCLUDE_DIRS ${LIBOBS_INCLUDE_DIR})
    set(LIBOBS_LIBRARIES ${LIBOBS_LIBRARY})
    
    if(NOT TARGET OBS::libobs)
        add_library(OBS::libobs UNKNOWN IMPORTED)
        set_target_properties(OBS::libobs PROPERTIES
            IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LIBOBS_INCLUDE_DIR LIBOBS_LIBRARY)