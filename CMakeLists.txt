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
# Build android_translation_layer (main ATL project)
# =============================================================================
set(CMAKE_INSTALL_PREFIX ${UNIFIED_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Library directory" FORCE)

# Pass unified directories to ATL build
set(UNIFIED_LIB_DIR ${UNIFIED_LIB_DIR} CACHE INTERNAL "Unified lib dir")
set(UNIFIED_JAVA_DIR ${UNIFIED_JAVA_DIR} CACHE INTERNAL "Unified java dir")

add_subdirectory(${CMAKE_SOURCE_DIR}/atl EXCLUDE_FROM_ALL)

# =============================================================================
# Aggregate targets
# =============================================================================
add_custom_target(all_build ALL DEPENDS
    wolfssl
    wolfcrypt
    dl_bio
    c_bio
    pthread_bio
    stdc++_bio
    art_standalone_all
    libandroid
    libtranslation_layer_main
    android-translation-layer
    api-impl.jar
    gstub.jar
    test_runner.jar
    framework-res.apk
)

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