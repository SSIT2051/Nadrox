cmake_minimum_required(VERSION 3.16)
project(android_translation_layer LANGUAGES C Java)

# Version
set(ATL_VERSION "0.1.0")
set(ATL_SOVERSION "0")

# =============================================================================
# Dependencies (provided by parent scope)
# =============================================================================
# These should be available from the root CMakeLists.txt:
# - wolfssl::wolfssl, wolfssl::wolfcrypt
# - bionic::dl_bio, bionic::c_bio, bionic::pthread_bio, bionic::stdcpp_bio
# - art::standalone, art::libart, art::libandroidfw, etc.
# - GTK4, JNI, Vulkan, OpenXR, Wayland, etc. (via pkg-config)

# =============================================================================
# Find system dependencies via pkg-config
# =============================================================================
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)
if(GTK4_VERSION VERSION_LESS "4.14")
    message(FATAL_ERROR "GTK4 >= 4.14 required")
endif()

find_package(JNI REQUIRED)

pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(OPENXR REQUIRED openxr)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(LIBPORTAL REQUIRED libportal)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(LIBAVCODEC REQUIRED libavcodec)
if(LIBAVCODEC_VERSION VERSION_LESS "59")
    message(FATAL_ERROR "libavcodec >= 59 required")
endif()
pkg_check_modules(LIBDRM REQUIRED libdrm)
pkg_check_modules(GUDEV REQUIRED gudev-1.0)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(WEBKITGTK REQUIRED webkitgtk-6.0)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)

find_package(OpenGL REQUIRED)
# find_package(EGL REQUIRED)  # No CMake config, use pkg-config
pkg_check_modules(EGL REQUIRED egl)

find_library(DL_LIBRARY dl)
find_package(Threads REQUIRED)
find_library(ASOUND_LIBRARY asound)

# =============================================================================
# GLib tools
# =============================================================================
find_program(GLIB_GENMARSHAL glib-genmarshal REQUIRED)
find_program(GLIB_DBUS_CODEGEN gdbus-codegen REQUIRED)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

# =============================================================================
# Wayland protocol generation
# =============================================================================
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_DIR ${CMAKE_SOURCE_DIR}/protocol)

# input-method-unstable-v2
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.h
           ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-client-protocol.h
           ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header
        ${PROTOCOL_DIR}/input-method-unstable-v2.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header
        ${PROTOCOL_DIR}/input-method-unstable-v2.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.h
    COMMAND ${WAYLAND_SCANNER} code
        ${PROTOCOL_DIR}/input-method-unstable-v2.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.c
    DEPENDS ${PROTOCOL_DIR}/input-method-unstable-v2.xml
    COMMENT "Generating Wayland protocol for input-method-unstable-v2"
)

# virtual-keyboard-unstable-v1
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.h
           ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-client-protocol.h
           ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header
        ${PROTOCOL_DIR}/virtual-keyboard-unstable-v1.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header
        ${PROTOCOL_DIR}/virtual-keyboard-unstable-v1.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.h
    COMMAND ${WAYLAND_SCANNER} code
        ${PROTOCOL_DIR}/virtual-keyboard-unstable-v1.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.c
    DEPENDS ${PROTOCOL_DIR}/virtual-keyboard-unstable-v1.xml
    COMMENT "Generating Wayland protocol for virtual-keyboard-unstable-v1"
)

set(WL_PROTO_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.h
)

set(WL_PROTO_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/input-method-unstable-v2-protocol.c
    ${CMAKE_CURRENT_BINARY_DIR}/protocol/virtual-keyboard-unstable-v1-protocol.c
)

# =============================================================================
# GDBus code generation
# =============================================================================
set(DBUS_XML_DIR ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mpris-dbus.c ${CMAKE_CURRENT_BINARY_DIR}/mpris-dbus.h
    COMMAND ${GLIB_DBUS_CODEGEN} --interface-prefix org.mpris --generate-c-code ${CMAKE_CURRENT_BINARY_DIR}/mpris-dbus ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.xml
    DEPENDS ${DBUS_XML_DIR}/org.mpris.MediaPlayer2.xml
    COMMENT "Generating MPRIS D-Bus code"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/portal-openuri.c ${CMAKE_CURRENT_BINARY_DIR}/portal-openuri.h
    COMMAND ${GLIB_DBUS_CODEGEN} --interface-prefix org.freedesktop.portal --generate-c-code ${CMAKE_CURRENT_BINARY_DIR}/portal-openuri ${DBUS_XML_DIR}/org.freedesktop.portal.OpenURI.xml
    DEPENDS ${DBUS_XML_DIR}/org.freedesktop.portal.OpenURI.xml
    COMMENT "Generating Portal OpenURI D-Bus code"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/portal-email.c ${CMAKE_CURRENT_BINARY_DIR}/portal-email.h
    COMMAND ${GLIB_DBUS_CODEGEN} --interface-prefix org.freedesktop.portal --generate-c-code ${CMAKE_CURRENT_BINARY_DIR}/portal-email ${DBUS_XML_DIR}/org.freedesktop.portal.Email.xml
    DEPENDS ${DBUS_XML_DIR}/org.freedesktop.portal.Email.xml
    COMMENT "Generating Portal Email D-Bus code"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-distributor.c ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-distributor.h
    COMMAND ${GLIB_DBUS_CODEGEN} --interface-prefix org.unifiedpush --generate-c-code ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-distributor ${DBUS_XML_DIR}/org.unifiedpush.Distributor1.xml
    DEPENDS ${DBUS_XML_DIR}/org.unifiedpush.Distributor1.xml
    COMMENT "Generating UnifiedPush Distributor D-Bus code"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-connector.c ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-connector.h
    COMMAND ${GLIB_DBUS_CODEGEN} --interface-prefix org.unifiedpush --generate-c-code ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-connector ${DBUS_XML_DIR}/org.unifiedpush.Connector1.xml
    DEPENDS ${DBUS_XML_DIR}/org.unifiedpush.Connector1.xml
    COMMENT "Generating UnifiedPush Connector D-Bus code"
)

set(DBUS_GENERATED_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/mpris-dbus.c
    ${CMAKE_CURRENT_BINARY_DIR}/portal-openuri.c
    ${CMAKE_CURRENT_BINARY_DIR}/portal-email.c
    ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-distributor.c
    ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-connector.c
)

set(DBUS_GENERATED_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/mpris-dbus.h
    ${CMAKE_CURRENT_BINARY_DIR}/portal-openuri.h
    ${CMAKE_CURRENT_BINARY_DIR}/portal-email.h
    ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-distributor.h
    ${CMAKE_CURRENT_BINARY_DIR}/unifiedpush-connector.h
)

# =============================================================================
# GLib marshal generation
# =============================================================================
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/marshal.c ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
    COMMAND ${GLIB_GENMARSHAL} --header --valist-marshallers --internal ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/marshal.list > ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
    COMMAND ${GLIB_GENMARSHAL} --body --valist-marshallers --internal ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/marshal.list > ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
    DEPENDS ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/marshal.list
    COMMENT "Generating GLib marshal code"
)

# =============================================================================
# GResource compilation
# =============================================================================
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/atl.gresource.c
    COMMAND ${GLIB_COMPILE_RESOURCES} --target=${CMAKE_CURRENT_BINARY_DIR}/atl.gresource.c --sourcedir=${CMAKE_SOURCE_DIR} --generate-source ${CMAKE_SOURCE_DIR}/src/main-executable/atl.gresource.xml
    DEPENDS ${CMAKE_SOURCE_DIR}/src/main-executable/atl.gresource.xml ${CMAKE_SOURCE_DIR}/src/main-executable/css/default-stylesheet.css
    COMMENT "Compiling GResource"
)

# =============================================================================
# libandroid shared library
# =============================================================================
add_library(libandroid SHARED
    ${CMAKE_SOURCE_DIR}/src/libandroid/asset_manager.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/bitmap.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/configuration.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/egl.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/input.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/looper.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/media.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/native_window.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/sensor.c
    ${CMAKE_SOURCE_DIR}/src/libandroid/trace.c
)

target_include_directories(libandroid PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libandroid>
    $<INSTALL_INTERFACE:include/libandroid>
    ${GTK4_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
    /tmp/fakevulkan/include
    ${CMAKE_SOURCE_DIR}/thirdparty/art_standalone/libandroidfw/include
    /home/ssit/Projects/Nadrox/thirdparty/art_standalone/libandroidfw/include
    /home/ssit/Projects/Nadrox/build/thirdparty/art_standalone/src/libandroidfw/include
)

target_compile_definitions(libandroid PRIVATE _LARGEFILE64_SOURCE)

target_link_libraries(libandroid PUBLIC
    ${GTK4_LIBRARIES}
    ${JNI_LIBRARIES}
    ${VULKAN_LIBRARIES}
    ${OPENXR_LIBRARIES}
)

target_link_options(libandroid PRIVATE -Wl,-z,lazy)

# =============================================================================
# libtranslation_layer_main shared library (main JNI library)
# =============================================================================
set(JNI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_app_NativeActivity.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_content_res_AssetManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_opengl_GLES20.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_opengl_GLUtils.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_os_Environment.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_os_MessageQueue.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_os_SystemClock.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_os_Vibrator.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_util_Log.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_view_Window.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_app_Activity.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_app_AlertDialog.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_app_Dialog.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_app_NotificationManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_app_WallpaperManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/android_inputmethodservice_InputMethodService.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/AssetInputStream.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/audio/android_media_AudioManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/audio/android_media_AudioRecord.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/audio/android_media_AudioTrack.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/audio/android_media_SoundPool.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/audio/audio.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/com_google_android_gles_jni_GLImpl.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content/android_atl_ATLMediaContentProvider.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content/android_content_ClipboardManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content/android_content_ContentResolver.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content/android_content_Context.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/content/android_content_res_XmlBlock.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/database/android_database_SQLiteCommon.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/database/android_database_SQLiteConnection.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/database/android_database_SQLiteGlobal.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/egl/com_google_android_gles_jni_EGLImpl.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/NinePatchPaintable.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_atl_GskCanvas.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_Bitmap.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_BitmapFactory.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_Matrix.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_Paint.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_Path.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_view_RenderNode.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_drawable_Drawable.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_drawable_DrawableContainer.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/graphics/android_graphics_drawable_NinePatchDrawable.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/location/android_location_LocationManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/media/android_media_MediaCodec.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/media/android_media_MediaPlayer.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/media/android_media_session_MediaSession.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/net/android_net_ConnectivityManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/os/android_atl_GVariantBuilderParcel.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/os/android_atl_GVariantIterParcel.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/os/android_os_Process.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/sensors/android_hardware_SensorManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/text/android_text_Layout.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/util.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/handle_cache.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/AndroidLayout.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/android_view_View.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/android_view_ViewGroup.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/android_view_ViewTreeObserver.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/android_view_WindowManagerImpl.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/gdkarrayimpl.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/views/android_view_inputmethod_InputMethodManager.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_view_SurfaceView.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_webkit_WebView.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_Button.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_CheckBox.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_CompoundButton.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_DatePicker.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_EditText.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_ImageButton.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_ImageView.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_PopupMenu.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_PopupWindow.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_Progressbar.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_RadioButton.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_ScrollView.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_Spinner.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_SeekBar.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_TextView.c
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni/widgets/android_widget_TimePicker.c
)

# Optional keyboard dialog (requires gtk4-layer-shell)
find_library(GTK4_LAYER_SHELL gtk4-layer-shell-0)
if(GTK4_LAYER_SHELL)
    list(APPEND JNI_SOURCES ${CMAKE_SOURCE_DIR}/src/api-impl-jni/app/android_atl_ATLKeyboardDialog.c)
    set(ATL_HAS_OSK 1)
endif()

add_library(libtranslation_layer_main SHARED
    ${JNI_SOURCES}
    ${DBUS_GENERATED_SOURCES}
    ${WL_PROTO_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
)

target_include_directories(libtranslation_layer_main PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/protocol
    ${CMAKE_SOURCE_DIR}/src/api-impl-jni
    ${CMAKE_SOURCE_DIR}/src/libandroid
    ${CMAKE_SOURCE_DIR}/thirdparty/art_standalone/libandroidfw/include
    /home/ssit/Projects/Nadrox/thirdparty/art_standalone/libandroidfw/include
    /home/ssit/Projects/Nadrox/build/thirdparty/art_standalone/src/libandroidfw/include
    /tmp/fakevulkan/include
    ${GTK4_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
    ${OPENXR_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${LIBPORTAL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${LIBAVCODEC_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
    ${GUDEV_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${WEBKITGTK_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${PROTOCOL_DIR}
)

target_compile_definitions(libtranslation_layer_main PRIVATE
    _LARGEFILE64_SOURCE
)

if(ATL_HAS_OSK)
    target_compile_definitions(libtranslation_layer_main PRIVATE ATL_HAS_OSK)
endif()

target_link_libraries(libtranslation_layer_main PUBLIC
    libandroid
    ${GTK4_LIBRARIES}
    ${GL_LIBRARIES}
    ${EGL_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${JNI_LIBRARIES}
    ${LIBPORTAL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LIBAVCODEC_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    ${GUDEV_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${WEBKITGTK_LIBRARIES}
    ${DL_LIBRARY}
    bionic::dl_bio
    bionic::c_bio
    Threads::Threads
    ${ASOUND_LIBRARY}
)

target_link_options(libtranslation_layer_main PRIVATE
    -Wl,-rpath,\$ORIGIN/:\$ORIGIN/../lib/art
)

set_target_properties(libtranslation_layer_main PROPERTIES
    SOVERSION ${ATL_SOVERSION}
    VERSION ${ATL_VERSION}
    POSITION_INDEPENDENT_CODE TRUE
    INSTALL_RPATH "\$ORIGIN/:\$ORIGIN/../lib/art:\$ORIGIN/../lib/java/dex/android_translation_layer/natives"
)

# =============================================================================
# Main executable
# =============================================================================
add_executable(android-translation-layer
    ${CMAKE_SOURCE_DIR}/src/main-executable/main.c
    ${CMAKE_SOURCE_DIR}/src/main-executable/actions.c
    ${CMAKE_SOURCE_DIR}/src/main-executable/back_button.c
    ${CMAKE_SOURCE_DIR}/src/main-executable/bionic_compat.c
    ${CMAKE_SOURCE_DIR}/src/main-executable/libc_bio_path_overrides.c
    ${CMAKE_CURRENT_BINARY_DIR}/atl.gresource.c
)

target_include_directories(android-translation-layer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main-executable
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GTK4_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
    ${LIBPORTAL_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
)

target_compile_definitions(android-translation-layer PRIVATE
    -DINSTALL_DATADIR=\"${CMAKE_INSTALL_DATADIR}\"
)

target_link_libraries(android-translation-layer PRIVATE
    ${GTK4_LIBRARIES}
    ${JNI_LIBRARIES}
    libtranslation_layer_main
    ${DL_LIBRARY}
    bionic::dl_bio
    bionic::c_bio
    ${LIBPORTAL_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
)

target_link_options(android-translation-layer PRIVATE
    -rdynamic
    -Wl,-rpath,\$ORIGIN/../lib/art:\$ORIGIN/../lib/java/dex/android_translation_layer/natives
)

set_target_properties(android-translation-layer PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../lib/art:\$ORIGIN/../lib/java/dex/android_translation_layer/natives"
)

# =============================================================================
# Java compilation and DEX conversion
# =============================================================================
find_program(DX_TOOL dx
    PATHS /opt/android-sdk/build-tools/34.0.0 /opt/android-sdk/build-tools/33.0.0 /opt/android-sdk/build-tools/32.0.0
    DOC "Android SDK dx tool"
)
if(NOT DX_TOOL)
    message(FATAL_ERROR "dx tool not found in Android SDK build-tools")
endif()

# Bootclasspath from art_standalone
set(BOOTCLASSPATH
    ${UNIFIED_LIB_DIR}/java/core-all_classes.jar:
    ${UNIFIED_LIB_DIR}/java/core-junit_classes.jar:
    ${UNIFIED_LIB_DIR}/java/junit-runner_classes.jar
)

# Helper function to compile Java sources to JAR
function(compile_java_to_jar TARGET_NAME SOURCE_DIR OUTPUT_JAR)
    file(GLOB_RECURSE JAVA_SOURCES ${SOURCE_DIR}/*.java)
    if(NOT JAVA_SOURCES)
        message(WARNING "No Java sources found in ${SOURCE_DIR}")
        return()
    endif()
    
    add_custom_target(${TARGET_NAME}-classes)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-classes.jar
        COMMAND ${CMAKE_JAVA_COMPILER}
            -source 1.8 -target 1.8
            -bootclasspath ${BOOTCLASSPATH}
            -d ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-classes
            ${JAVA_SOURCES}
        COMMAND jar cf ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-classes.jar -C ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-classes .
        DEPENDS libtranslation_layer_main
        COMMENT "Compiling ${TARGET_NAME} Java sources"
    )
    
    add_custom_target(${TARGET_NAME} DEPENDS ${TARGET_NAME}-classes)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_JAR}
        COMMAND ${DX_TOOL} --dex --incremental --output=${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_JAR} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-classes.jar
        DEPENDS ${TARGET_NAME}-classes
        COMMENT "Converting ${TARGET_NAME} to DEX"
    )
endfunction()

# api-impl.jar
compile_java_to_jar(api-impl ${CMAKE_SOURCE_DIR}/src/api-impl api-impl.jar)

# gstub.jar
compile_java_to_jar(gstub ${CMAKE_SOURCE_DIR}/src/gstub gstub.jar)

# test-runner.jar
compile_java_to_jar(test-runner ${CMAKE_SOURCE_DIR}/src/test-runner test_runner.jar)

# =============================================================================
# framework-res.apk (prebuilt)
# =============================================================================
add_custom_target(framework-res.apk)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/framework-res.apk
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/res/framework-res/framework-res.apk ${CMAKE_CURRENT_BINARY_DIR}/framework-res.apk
    COMMENT "Copying framework-res.apk"
)

# =============================================================================
# Install rules
# =============================================================================
install(TARGETS libandroid libtranslation_layer_main android-translation-layer
    LIBRARY DESTINATION lib/java/dex/android_translation_layer/natives
    RUNTIME DESTINATION bin
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/api-impl.jar
    ${CMAKE_CURRENT_BINARY_DIR}/gstub.jar
    ${CMAKE_CURRENT_BINARY_DIR}/test_runner.jar
    DESTINATION lib/java/dex/android_translation_layer
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/res/fonts.xml
    DESTINATION share/atl/system/etc
)

# =============================================================================
# Export targets
# =============================================================================
export(TARGETS libandroid libtranslation_layer_main android-translation-layer
    FILE "${CMAKE_CURRENT_BINARY_DIR}/android_translation_layer-targets.cmake"
    NAMESPACE atl::
)