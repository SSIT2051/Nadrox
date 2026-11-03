cmake_minimum_required(VERSION 3.16)
project(art_standalone LANGUAGES C CXX)

# Version
set(ART_STANDALONE_VERSION "0.0.0")

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    set(ART_HOST_ARCH "linux-x86")
    set(ART_HOST_LIBDIR "lib64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(ART_HOST_ARCH "linux-arm64")
    set(ART_HOST_LIBDIR "lib64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "i386" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
    set(ART_HOST_ARCH "linux-x86")
    set(ART_HOST_LIBDIR "lib")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7l")
    set(ART_HOST_ARCH "linux-arm")
    set(ART_HOST_LIBDIR "lib")
else()
    message(WARNING "Unknown architecture ${CMAKE_SYSTEM_PROCESSOR}, defaulting to linux-x86")
    set(ART_HOST_ARCH "linux-x86")
    set(ART_HOST_LIBDIR "lib64")
endif()

# Paths
set(ART_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ART_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(ART_OUT_DIR "${ART_SOURCE_DIR}/out/host/${ART_HOST_ARCH}")

# Install paths
set(ART_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(ART_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
set(ART_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(ART_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)

# Java configuration
find_package(Java REQUIRED COMPONENTS Development)
if(Java_JAVAC_EXECUTABLE)
    set(ART_JAVAC ${Java_JAVAC_EXECUTABLE})
    message(STATUS "Using javac: ${ART_JAVAC}")
else()
    message(FATAL_ERROR "javac not found")
endif()

# dx tool from Android SDK (for dex conversion)
find_program(DX_TOOL dx
    PATHS /opt/android-sdk/build-tools/34.0.0 /opt/android-sdk/build-tools/33.0.0 /opt/android-sdk/build-tools/32.0.0
    DOC "Android SDK dx tool for .class to .dex conversion"
)
if(NOT DX_TOOL)
    message(WARNING "dx tool not found in Android SDK build-tools. dex conversion will fail.")
endif()

# Ant for build (used by some AOSP make rules)
find_program(ANT_TOOL ant DOC "Apache Ant")
if(NOT ANT_TOOL)
    message(WARNING "ant not found. Some build steps may fail.")
endif()

# =============================================================================
# Build art_standalone using ExternalProject (fetches full AOSP build system)
# =============================================================================
include(ExternalProject)

# bionic_translation library path (needed for -ldl_bio etc.)
set(BIONIC_LIB_DIR "${CMAKE_BINARY_DIR}/thirdparty/bionic_translation")

# Check if local source has the AOSP build system
set(ART_HAS_BUILD_SYSTEM FALSE)
if(EXISTS "${ART_SOURCE_DIR}/build/core/main.mk")
    set(ART_HAS_BUILD_SYSTEM TRUE)
    message(STATUS "art_standalone: Using local source with AOSP build system")
else()
    message(STATUS "art_standalone: Local source missing AOSP build system, will fetch from upstream")
endif()

if(ART_HAS_BUILD_SYSTEM)
    # Use local source with existing Makefile
    configure_file(
        ${ART_SOURCE_DIR}/art-standalone.pc.in
        ${ART_BUILD_DIR}/art-standalone.pc
        @ONLY
    )

    add_custom_target(art_standalone_build
        COMMAND ${CMAKE_COMMAND} -E env
            JAVA_HOME=$ENV{JAVA_HOME}
            PATH=$ENV{PATH}:${CMAKE_BINARY_DIR}/bin
            LIBRARY_PATH=${BIONIC_LIB_DIR}:$ENV{LIBRARY_PATH}
            LD_LIBRARY_PATH=${BIONIC_LIB_DIR}:$ENV{LD_LIBRARY_PATH}
            make -C ${ART_SOURCE_DIR} -j${NPROC}
            ____art_all
        WORKING_DIRECTORY ${ART_SOURCE_DIR}
        COMMENT "Building art_standalone with Makefile..."
        VERBATIM
    )
else()
    # Fetch and build from upstream using ExternalProject
    ExternalProject_Add(art_standalone_external
        GIT_REPOSITORY https://gitlab.com/android_translation_layer/art_standalone.git
        GIT_TAG master
        SOURCE_DIR "${ART_BUILD_DIR}/src"
        BINARY_DIR "${ART_BUILD_DIR}/src"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E env
            JAVA_HOME=/usr/lib/jvm/java-21-openjdk
            PATH=/usr/lib/jvm/java-21-openjdk/bin:$ENV{PATH}:${CMAKE_BINARY_DIR}/bin
            LIBRARY_PATH=${BIONIC_LIB_DIR}:$ENV{LIBRARY_PATH}
            LD_LIBRARY_PATH=${BIONIC_LIB_DIR}:$ENV{LD_LIBRARY_PATH}
            make -j${NPROC}
            ____art_all
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_BUILD ON
        LOG_INSTALL ON
    )

    add_custom_target(art_standalone_build DEPENDS art_standalone_external)
    set(ART_OUT_DIR "${ART_BUILD_DIR}/src/out/host/${ART_HOST_ARCH}")
endif()

# Create imported targets for the built artifacts
# These will be populated after the build

# =============================================================================
# Native libraries
# =============================================================================
set(ART_NATIVE_LIBS
    libandroidfw.so
    libart.so
    libart-compiler.so
    libart-dexlayout.so
    libartbase.so
    libartpalette.so
    libbacktrace.so
    libbase.so
    libcutils.so
    libdexfile.so
    liblog.so
    libnativebridge.so
    libprofile.so
    libsigchain.so
    libunwind.so
    libutils.so
    libziparchive.so
)

foreach(lib ${ART_NATIVE_LIBS})
    add_library(art_${lib} SHARED IMPORTED GLOBAL)
    set_target_properties(art_${lib} PROPERTIES
        IMPORTED_LOCATION "${ART_OUT_DIR}/${ART_HOST_LIBDIR}/${lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${ART_SOURCE_DIR}/libandroidfw/include"
    )
endforeach()

# =============================================================================
# JNI libraries (installed to java/dex/art/natives)
# =============================================================================
set(ART_JNI_LIBS
    libjavacore.so
    libnativehelper.so
    libopenjdk.so
    libopenjdkjvm.so
    libwolfssljni.so
)

foreach(lib ${ART_JNI_LIBS})
    add_library(art_jni_${lib} SHARED IMPORTED GLOBAL)
    set_target_properties(art_jni_${lib} PROPERTIES
        IMPORTED_LOCATION "${ART_OUT_DIR}/${ART_HOST_LIBDIR}/${lib}"
    )
endforeach()

# =============================================================================
# Binaries
# =============================================================================
# dex2oat
add_executable(art_dex2oat IMPORTED GLOBAL)
set_target_properties(art_dex2oat PROPERTIES
    IMPORTED_LOCATION "${ART_OUT_DIR}/bin/dex2oat"
)

# dx
add_executable(art_dx IMPORTED GLOBAL)
set_target_properties(art_dx PROPERTIES
    IMPORTED_LOCATION "${ART_OUT_DIR}/bin/dx"
)

# dalvikvm (32-bit)
add_executable(art_dalvikvm IMPORTED GLOBAL)
set_target_properties(art_dalvikvm PROPERTIES
    IMPORTED_LOCATION "${ART_OUT_DIR}/bin/dalvikvm"
)

# dalvikvm64 (64-bit)
add_executable(art_dalvikvm64 IMPORTED GLOBAL)
set_target_properties(art_dalvikvm64 PROPERTIES
    IMPORTED_LOCATION "${ART_OUT_DIR}/bin/dalvikvm64"
)

# adbd
add_executable(art_adbd IMPORTED GLOBAL)
set_target_properties(art_adbd PROPERTIES
    IMPORTED_LOCATION "${ART_OUT_DIR}/bin/adbd"
)

# =============================================================================
# JAR files
# =============================================================================
set(ART_HOSTDEX_JARS
    apachehttp-hostdex.jar
    apache-xml-hostdex.jar
    bouncycastle-hostdex.jar
    core-junit-hostdex.jar
    core-libart-hostdex.jar
    core-oj-hostdex.jar
    hamcrest-hostdex.jar
    junit-runner-hostdex.jar
    okhttp-hostdex.jar
    wolfssljni-hostdex.jar
)

foreach(jar ${ART_HOSTDEX_JARS})
    add_custom_target(art_jar_${jar} DEPENDS art_standalone_build)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${jar}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ART_OUT_DIR}/framework/${jar}
            ${CMAKE_BINARY_DIR}/${jar}
        DEPENDS art_standalone_build
        COMMENT "Copying ${jar}..."
    )
endforeach()

# Core classes JARs (from common obj directory)
set(ART_CORE_JARS
    core-all_classes.jar
    core-junit_classes.jar
    junit-runner_classes.jar
)

foreach(jar ${ART_CORE_JARS})
    # Extract base name without .jar suffix
    string(REGEX REPLACE "\\.jar$" "" jar_base ${jar})
    add_custom_target(art_jar_${jar} DEPENDS art_standalone_build)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${jar}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ART_SOURCE_DIR}/out/host/common/obj/JAVA_LIBRARIES/${jar_base}_hostdex_intermediates/classes.jar
            ${CMAKE_BINARY_DIR}/${jar}
        DEPENDS art_standalone_build
        COMMENT "Copying ${jar}..."
    )
endforeach()

# dx.jar
add_custom_target(art_jar_dx.jar DEPENDS art_standalone_build)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/dx.jar
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ART_OUT_DIR}/framework/dx.jar
        ${CMAKE_BINARY_DIR}/dx.jar
    DEPENDS art_standalone_build
    COMMENT "Copying dx.jar..."
)

# =============================================================================
# Header files
# =============================================================================
add_custom_target(art_headers DEPENDS art_standalone_build)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/include/androidfw/androidfw_c_api.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/androidfw
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ART_SOURCE_DIR}/libandroidfw/include/androidfw/androidfw_c_api.h
        ${CMAKE_BINARY_DIR}/include/androidfw/
    DEPENDS art_standalone_build
    COMMENT "Copying androidfw headers..."
)

# =============================================================================
# Aggregate target
# =============================================================================
add_custom_target(art_standalone_all DEPENDS
    art_standalone_build
    ${ART_CORE_JARS}
    dx.jar
    ${ART_HOSTDEX_JARS}
    art_headers
)

# =============================================================================
# Install rules
# =============================================================================
# Install native libraries using FILES (imported targets can't be installed with TARGETS)
foreach(lib ${ART_NATIVE_LIBS})
    install(FILES "${ART_OUT_DIR}/${ART_HOST_LIBDIR}/${lib}"
        DESTINATION lib/art
        RENAME ${lib}
    )
endforeach()

foreach(lib ${ART_JNI_LIBS})
    install(FILES "${ART_OUT_DIR}/${ART_HOST_LIBDIR}/${lib}"
        DESTINATION lib/java/dex/art/natives
        RENAME ${lib}
    )
endforeach()

# Install binaries
foreach(bin dex2oat dx dalvikvm dalvikvm64 adbd)
    install(FILES "${ART_OUT_DIR}/bin/${bin}"
        DESTINATION bin
        RENAME ${bin}
    )
endforeach()

install(FILES
    ${CMAKE_BINARY_DIR}/core-all_classes.jar
    ${CMAKE_BINARY_DIR}/core-junit_classes.jar
    ${CMAKE_BINARY_DIR}/junit-runner_classes.jar
    DESTINATION lib/java
)

install(FILES
    ${CMAKE_BINARY_DIR}/dx.jar
    DESTINATION lib/java
)

install(FILES
    ${CMAKE_BINARY_DIR}/apachehttp-hostdex.jar
    ${CMAKE_BINARY_DIR}/apache-xml-hostdex.jar
    ${CMAKE_BINARY_DIR}/bouncycastle-hostdex.jar
    ${CMAKE_BINARY_DIR}/core-junit-hostdex.jar
    ${CMAKE_BINARY_DIR}/core-libart-hostdex.jar
    ${CMAKE_BINARY_DIR}/core-oj-hostdex.jar
    ${CMAKE_BINARY_DIR}/hamcrest-hostdex.jar
    ${CMAKE_BINARY_DIR}/junit-runner-hostdex.jar
    ${CMAKE_BINARY_DIR}/okhttp-hostdex.jar
    ${CMAKE_BINARY_DIR}/wolfssljni-hostdex.jar
    DESTINATION lib/java/dex/art
)

install(FILES
    ${CMAKE_BINARY_DIR}/include/androidfw/androidfw_c_api.h
    DESTINATION include/androidfw
)

install(FILES
    ${ART_BUILD_DIR}/art-standalone.pc
    DESTINATION lib/pkgconfig
    RENAME art-standalone.pc
)

# =============================================================================
# Export targets
# =============================================================================
# Note: Cannot export imported targets, export is handled by parent scope
# export(TARGETS ...)