find_package(PkgConfig)
pkg_check_modules (LIBVA REQUIRED libva>=1.0.0)
pkg_check_modules (LIBVA_DRM REQUIRED libva-drm)

# set (LIBVA_LIBS "")

# if (LIBVA_FOUND)
#         list (APPEND LIBVA_LIBS ${LIBVA_LIBRARIES})
#         include_directories (${LIBVA_INCLUDE_DIRS})
#         link_directories(${LIBVA_LIBRARY_DIRS})
# endif ()

# set (${libva} ${LIBVA_LIBS} PARENT_SCOPE)
