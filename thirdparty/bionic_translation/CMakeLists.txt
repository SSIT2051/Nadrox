cmake_minimum_required(VERSION 3.16)
project(bionic_translation LANGUAGES C CXX)

# Version
set(BIONIC_TRANSLATION_VERSION "0.0.1")
set(BIONIC_TRANSLATION_SOVERSION "0")

# Common compile options
set(BIONIC_COMMON_C_ARGS
    -fPIC
    -D_GNU_SOURCE
    -D_LARGEFILE64_SOURCE
)

# Find dependencies
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
# find_package(LibUnwind REQUIRED)  # No CMake config, use pkg-config
# find_package(LibElf REQUIRED)  # No CMake config, use pkg-config

# libunwind via pkg-config
pkg_check_modules(LIBUNWIND REQUIRED libunwind)

# libelf via pkg-config
pkg_check_modules(LIBELF REQUIRED libelf)

# EGL via pkg-config (needed for eglGetProcAddress)
pkg_check_modules(EGL REQUIRED egl)

# libdl
find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "libdl not found")
endif()

# libbsd
find_library(BSD_LIBRARY bsd)
if(NOT BSD_LIBRARY)
    message(FATAL_ERROR "libbsd not found")
endif()

# math library
find_library(M_LIBRARY m)

# =============================================================================
# Config library
# =============================================================================
add_library(bionic_config STATIC
    shared/config/config.c
)
target_include_directories(bionic_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/config
)
target_compile_options(bionic_config PRIVATE ${BIONIC_COMMON_C_ARGS})

# =============================================================================
# Wrapper library
# =============================================================================
add_library(bionic_wrapper STATIC
    wrapper/wrapper.c
)
target_compile_options(bionic_wrapper PRIVATE
    ${BIONIC_COMMON_C_ARGS}
    -fvisibility=hidden
)

# =============================================================================
# pthread_bio
# =============================================================================
add_library(pthread_bio SHARED
    pthread_wrapper/libpthread.c
)
target_compile_options(pthread_bio PRIVATE
    ${BIONIC_COMMON_C_ARGS}
    -fno-exceptions
)
target_link_libraries(pthread_bio PRIVATE ${DL_LIBRARY} Threads::Threads)
set_target_properties(pthread_bio PROPERTIES
    SOVERSION ${BIONIC_TRANSLATION_SOVERSION}
    VERSION ${BIONIC_TRANSLATION_VERSION}
    POSITION_INDEPENDENT_CODE TRUE
)
target_link_options(pthread_bio PRIVATE
    -Wl,--no-as-needed
)

# =============================================================================
# dl_bio
# =============================================================================
add_library(dl_bio SHARED
    linker/dlfcn.c
    linker/linker.c
    linker/linker_environ.c
    linker/linker_tls.c
    linker/rt.c
    linker/strlcpy.c
)
target_include_directories(dl_bio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper
    ${LIBUNWIND_INCLUDE_DIRS}
    ${LIBELF_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
)
target_compile_options(dl_bio PRIVATE
    ${BIONIC_COMMON_C_ARGS}
    -DLINKER_DEBUG=1
    -DVERBOSE_FUNCTIONS=1
    -Wno-parentheses
)
target_link_libraries(dl_bio PRIVATE
    ${DL_LIBRARY}
    ${BSD_LIBRARY}
    ${LIBUNWIND_LIBRARIES}
    ${LIBELF_LIBRARIES}
    ${EGL_LIBRARIES}
    bionic_wrapper
    bionic_config
)
set_target_properties(dl_bio PROPERTIES
    SOVERSION "0.0.1"
    VERSION ${BIONIC_TRANSLATION_VERSION}
    POSITION_INDEPENDENT_CODE TRUE
)
target_link_options(dl_bio PRIVATE
    -Wl,--no-as-needed
    -lpthread
    -Wl,--gc-sections
)

# =============================================================================
# c_bio (libc_bio)
# =============================================================================
# Architecture-specific hardcoded symbols
set(LIBC_BIO_HARDCODED_SYMBOLS
    "-Wl,--defsym,__fe_dfl_env=-1"
)

# 32-bit specific symbols
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" OR CMAKE_C_FLAGS MATCHES "-m32")
    list(APPEND LIBC_BIO_HARDCODED_SYMBOLS "-Wl,--defsym,__bionic_brk=0x123")
endif()

add_library(c_bio SHARED
    libc/libc.c
    libc/libc-chk.c
    libc/libc-math.c
    libc/libc-misc.c
    libc/libc-musl.c
    libc/libc-signal.c
    libc/libc-stdio.c
    libc/libc-sha1.c
    libc/libc-open-overrides.c
)
target_compile_options(c_bio PRIVATE
    ${BIONIC_COMMON_C_ARGS}
    -DPAGESIZE=4096
)
target_include_directories(c_bio PRIVATE
    ${LIBUNWIND_INCLUDE_DIRS}
)
target_link_libraries(c_bio PRIVATE
    ${BSD_LIBRARY}
    ${LIBUNWIND_LIBRARIES}
    ${M_LIBRARY}
    bionic_wrapper
    pthread_bio
)
set_target_properties(c_bio PROPERTIES
    SOVERSION ${BIONIC_TRANSLATION_SOVERSION}
    VERSION ${BIONIC_TRANSLATION_VERSION}
    POSITION_INDEPENDENT_CODE TRUE
)
target_link_options(c_bio PRIVATE
    ${LIBC_BIO_HARDCODED_SYMBOLS}
    -Wl,-wrap,_IO_file_xsputn
    -Wl,--no-as-needed
    "$<TARGET_FILE:pthread_bio>"
)

# =============================================================================
# stdc++_bio
# =============================================================================
add_library(stdc++_bio SHARED
    libstdc++_standalone/new.cpp
    libstdc++_standalone/__cxa_pure_virtual.cpp
    libstdc++_standalone/__cxa_guard.cpp
)
target_include_directories(stdc++_bio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libstdc++_standalone/include
)
target_compile_options(stdc++_bio PRIVATE
    --std=c++23
)
set_target_properties(stdc++_bio PROPERTIES
    SOVERSION ${BIONIC_TRANSLATION_SOVERSION}
    VERSION ${BIONIC_TRANSLATION_VERSION}
    POSITION_INDEPENDENT_CODE TRUE
)

# =============================================================================
# Install targets
# =============================================================================
install(TARGETS dl_bio c_bio pthread_bio stdc++_bio
    LIBRARY DESTINATION lib
)

install(FILES
    cfg.d/bionic_translation.cfg
    DESTINATION share/bionic_translation/cfg.d
)

# =============================================================================
# Export targets
# =============================================================================
export(TARGETS dl_bio c_bio pthread_bio stdc++_bio
    FILE "${CMAKE_BINARY_DIR}/bionic_translation-targets.cmake"
    NAMESPACE bionic::
)