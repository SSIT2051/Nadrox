cmake_minimum_required(VERSION 3.20)
project(android_translation_layer_unified VERSION 0.1.0 LANGUAGES C CXX Java)

# =============================================================================
# Global configuration
# =============================================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Unified install prefix
set(UNIFIED_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)
set(UNIFIED_LIB_DIR ${UNIFIED_INSTALL_PREFIX}/lib)
set(UNIFIED_BIN_DIR ${UNIFIED_INSTALL_PREFIX}/bin)
set(UNIFIED_INCLUDE_DIR ${UNIFIED_INSTALL_PREFIX}/include)
set(UNIFIED_PKGCONFIG_DIR ${UNIFIED_LIB_DIR}/pkgconfig)
set(UNIFIED_JAVA_DIR ${UNIFIED_LIB_DIR}/java)

# Parallel jobs
include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 4)
endif()

# Java Configuration
find_package(Java REQUIRED COMPONENTS Development)
if(Java_JAVAC_EXECUTABLE)
    set(JAVA_HOME_DIR ${JAVA_HOME})
    message(STATUS "Using Java: ${JAVA_HOME_DIR}")
else()
    message(FATAL_ERROR "Java Development Kit not found")
endif()

# dx tool from Android SDK
find_program(DX_TOOL dx
    PATHS /opt/android-sdk/build-tools/34.0.0 /opt/android-sdk/build-tools/33.0.0 /opt/android-sdk/build-tools/32.0.0
    DOC "Android SDK dx tool"
)
if(NOT DX_TOOL)
    message(WARNING "dx tool not found - DEX conversion will fail")
endif()

# =============================================================================
# FEX-Emu Integration (for x86_64 guest translation on aarch64 host)
# =============================================================================
include(FetchContent)

option(BUILD_FEX_EMU "Build FEX-Emu for x86_64 guest translation (requires network)" OFF)

# FEX-Emu provides x86_64 guest translation (runs on aarch64 host).
# On x86_64 hosts with ENABLE_X86_HOST_DEBUG, it builds the x86_64 JIT/interpreter.
# ARM translation is NOT provided by FEX-Emu; our custom engine handles ARM->x86_64.
if(BUILD_FEX_EMU)
    FetchContent_Declare(
        fex_emu
        GIT_REPOSITORY https://github.com/FEX-Emu/FEX.git
        GIT_TAG FEX-2407  # Stable release tag
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    # Configure FEX-Emu build options
    set(FEX_BUILD_DOCS OFF CACHE BOOL "Disable docs")
    set(FEX_BUILD_TESTS OFF CACHE BOOL "Disable tests")
    set(FEX_BUILD_INTERCEPTOR ON CACHE BOOL "Enable interceptor library")
    set(FEX_BUILD_ROOTFS OFF CACHE BOOL "Disable rootfs")
    set(FEX_CONFIGURATION "Release" CACHE STRING "Build type")
    # FEX-Emu rejects x86_64 hosts unless ENABLE_X86_HOST_DEBUG is enabled
    set(ENABLE_X86_HOST_DEBUG ON CACHE BOOL "Enable compiling FEX-Emu on x86_64 host")

    FetchContent_MakeAvailable(fex_emu)

    message(STATUS "FEX-Emu integration enabled for x86_64 guest translation")
else()
    message(STATUS "FEX-Emu disabled (ARM->x86_64 translation handled by custom engine)")
endif()

# =============================================================================
# box86/box64 removed; ARM translation handled by custom engine (atl_translation)
# =============================================================================
set(BOX86_FOUND FALSE)
set(BOX64_FOUND FALSE)

# Create output directories
file(MAKE_DIRECTORY ${UNIFIED_BIN_DIR})
file(MAKE_DIRECTORY ${UNIFIED_LIB_DIR})
file(MAKE_DIRECTORY ${UNIFIED_INCLUDE_DIR})
file(MAKE_DIRECTORY ${UNIFIED_PKGCONFIG_DIR})
file(MAKE_DIRECTORY ${UNIFIED_JAVA_DIR}/dex/art)
file(MAKE_DIRECTORY ${UNIFIED_JAVA_DIR}/dex/art/natives)
file(MAKE_DIRECTORY ${UNIFIED_JAVA_DIR}/dex/android_translation_layer)
file(MAKE_DIRECTORY ${UNIFIED_JAVA_DIR}/dex/android_translation_layer/natives)

message(STATUS "Unified install prefix: ${UNIFIED_INSTALL_PREFIX}")
message(STATUS "Parallel jobs: ${NPROC}")

# =============================================================================
# Build wolfSSL (uses upstream CMakeLists.txt)
# =============================================================================
set(WOLFSSL_TLS13 ON CACHE BOOL "Enable TLS 1.3")
set(WOLFSSL_DTLS ON CACHE BOOL "Enable DTLS")
set(WOLFSSL_SRTP OFF CACHE BOOL "Disable DTLS-SRTP")
set(WOLFSSL_OPENSSLEXTRA ON CACHE BOOL "Enable OpenSSL extra API")
set(WOLFSSL_OPENSSLALL OFF CACHE BOOL "Disable OpenSSL all API")
set(WOLFSSL_CURVE25519 ON CACHE BOOL "Enable Curve25519")
set(WOLFSSL_ED25519 ON CACHE BOOL "Enable Ed25519")
set(WOLFSSL_SHA512 ON CACHE BOOL "Enable SHA-512")
set(WOLFSSL_SHA384 ON CACHE BOOL "Enable SHA-384")
set(WOLFSSL_AESGCM ON CACHE BOOL "Enable AES-GCM")
set(WOLFSSL_AESCBC ON CACHE BOOL "Enable AES-CBC")
set(WOLFSSL_HKDF ON CACHE BOOL "Enable HKDF")
set(WOLFSSL_RSA_PSS ON CACHE BOOL "Enable RSA-PSS")
set(WOLFSSL_JNI ON CACHE BOOL "Enable JNI support")
set(WOLFSSL_INSTALL ON CACHE BOOL "Create install targets")
set(WOLFSSL_STATIC OFF CACHE BOOL "Build shared libraries")
set(WOLFSSL_DEBUG OFF CACHE BOOL "Disable debug build")
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

add_subdirectory(thirdparty/wolfssl EXCLUDE_FROM_ALL)

# Make wolfSSL targets available globally
# wolfSSL's CMakeLists.txt already creates wolfssl::wolfssl alias
# add_library(wolfssl::wolfcrypt ALIAS wolfcrypt)  # wolfcrypt is not a separate target

# =============================================================================
# Build bionic_translation
# =============================================================================
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

add_subdirectory(thirdparty/bionic_translation EXCLUDE_FROM_ALL)

# Make bionic targets available globally
add_library(bionic::dl_bio ALIAS dl_bio)
add_library(bionic::c_bio ALIAS c_bio)
add_library(bionic::pthread_bio ALIAS pthread_bio)
add_library(bionic::stdcpp_bio ALIAS stdc++_bio)

# =============================================================================
# Build art_standalone
# =============================================================================
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

add_subdirectory(thirdparty/art_standalone EXCLUDE_FROM_ALL)

# =============================================================================
# Build custom ARM translation engine
# =============================================================================
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/translation EXCLUDE_FROM_ALL)

# Pass translation engine status to ATL build
set(ATL_TRANSLATION_FOUND TRUE CACHE INTERNAL "Custom ARM translation engine found")

# =============================================================================
# Build android_translation_layer (main ATL project)
# =============================================================================
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

# Pass unified directories to ATL build
set(UNIFIED_LIB_DIR ${UNIFIED_LIB_DIR} CACHE INTERNAL "Unified lib dir")
set(UNIFIED_JAVA_DIR ${UNIFIED_JAVA_DIR} CACHE INTERNAL "Unified java dir")

# Pass translation backend status to ATL build (box86/box64 removed)
set(BOX86_FOUND FALSE CACHE INTERNAL "box86 found")
set(BOX64_FOUND FALSE CACHE INTERNAL "box64 found")

add_subdirectory(${CMAKE_SOURCE_DIR}/atl EXCLUDE_FROM_ALL)

# Ensure art_standalone artifacts are built before libtranslation_layer_main links
add_dependencies(libtranslation_layer_main art_standalone_build)

# =============================================================================
# ARM Translation Tests
# =============================================================================
add_subdirectory(${CMAKE_SOURCE_DIR}/tests/arm_translation EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_SOURCE_DIR}/tests/elf EXCLUDE_FROM_ALL)

# =============================================================================
# Aggregate targets
# =============================================================================
add_custom_target(all_build ALL DEPENDS
    wolfssl
    dl_bio
    c_bio
    pthread_bio
    stdc++_bio
    art_standalone_all
    libandroid
    libtranslation_layer_main
    android-translation-layer.bin
    android-translation-layer
    api-impl
)
add_dependencies(all_build atl_translation test_arm_translation)

# =============================================================================
# Install target
# =============================================================================
add_custom_target(install_all
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${UNIFIED_INSTALL_PREFIX}
    DEPENDS all_build
    COMMENT "Installing all components..."
)

# =============================================================================
# Clean target
# =============================================================================
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/thirdparty/wolfssl
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/thirdparty/bionic_translation
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/thirdparty/art_standalone
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/atl
    COMMENT "Cleaning all build directories..."
)

# =============================================================================
# Package configuration
# =============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/thirdparty/art_standalone/art-standalone.pc.in
    ${UNIFIED_PKGCONFIG_DIR}/art-standalone.pc
    @ONLY
)

# =============================================================================
# Packaging install rules (for native Linux packages)
# =============================================================================
if(CMAKE_INSTALL_PREFIX MATCHES "^/usr")
    # Desktop entry
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/nadrox.desktop
        DESTINATION share/applications
    )
    
    # MIME type
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/nadrox-mime.xml
        DESTINATION share/mime/packages
        RENAME nadrox.xml
    )
    
    # Icons
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/icons/hicolor/scalable/apps/nadrox.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/icons/hicolor/64x64/apps/nadrox.svg
        DESTINATION share/icons/hicolor/64x64/apps
        RENAME nadrox-apk.svg
    )
    
    # Pacman hook (Arch Linux)
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/nadrox.hook
        DESTINATION share/libalpm/hooks
    )
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Unified CMake Build Configuration ===")
message(STATUS "Components:")
message(STATUS "  - wolfSSL (CMake native)")
message(STATUS "  - bionic_translation (CMake native)")
message(STATUS "  - art_standalone (wrapped Makefile)")
message(STATUS "  - android_translation_layer (CMake native)")
message(STATUS "")
message(STATUS "Build: cmake --build build")
message(STATUS "Install: cmake --build build --target install_all")
message(STATUS "Clean: cmake --build build --target clean_all")
message(STATUS "")