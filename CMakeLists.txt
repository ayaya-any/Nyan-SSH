cmake_minimum_required(VERSION 3.28)

project(NyanSSH VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Qt ---------------------------------------------------------------------
if(NOT DEFINED QT_ROOT)
    foreach(_qt_candidate
            "D:/Qt/6.12.0/msvc2022_64"
            "C:/Qt/6.12.0/msvc2022_64"
            "D:/Qt/6.12.0/msvc2019_64")
        if(EXISTS "${_qt_candidate}/lib/cmake/Qt6/Qt6Config.cmake")
            set(QT_ROOT "${_qt_candidate}" CACHE PATH "Qt 6 installation prefix" FORCE)
            break()
        endif()
    endforeach()
endif()
if(QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Svg)
qt_standard_project_setup(REQUIRES 6.5)
set(CMAKE_AUTORCC ON)

# ---- OpenSSL ----------------------------------------------------------------
# OpenSSL is the portable libssh2 crypto backend.  On Windows, install it with
# vcpkg (or point OPENSSL_ROOT_DIR at a distribution) so the DLL can be bundled.
find_package(OpenSSL 3.0 REQUIRED)
set(NYAN_OPENSSL_SHARED FALSE)
if(WIN32)
    # vcpkg's OpenSSLConfig reports the import library as IMPORTED_LOCATION,
    # so locate the runtime DLL next to the include directory instead.
    get_filename_component(_nyan_openssl_prefix "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    file(GLOB _nyan_openssl_crypto_dlls "${_nyan_openssl_prefix}/bin/libcrypto-*.dll")
    if(_nyan_openssl_crypto_dlls)
        list(GET _nyan_openssl_crypto_dlls 0 NYAN_OPENSSL_CRYPTO_DLL)
        set(NYAN_OPENSSL_SHARED TRUE)
    endif()
endif()
function(nyan_copy_openssl_runtime target)
    if(NYAN_OPENSSL_SHARED)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NYAN_OPENSSL_CRYPTO_DLL}"
                    "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

# ---- libssh2 via FetchContent ----------------------------------------------
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries" FORCE)
set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "libssh2 crypto backend" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build libssh2 examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build libssh2 tests" FORCE)
set(ENABLE_ZLIB_COMPRESSION OFF CACHE BOOL "Enable libssh2 zlib compression" FORCE)
set(CMAKE_UNITY_BUILD OFF)
FetchContent_Declare(libssh2
    URL https://github.com/libssh2/libssh2/releases/download/libssh2-1.11.1/libssh2-1.11.1.tar.gz
    URL_HASH SHA256=d9ec76cbe34db98eec3539fe2c899d26b0c837cb3eb466a56b0f109cabf658f7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
)
# Set -DFETCHCONTENT_SOURCE_DIR_LIBSSH2=<dir> for an offline build against an
# already extracted libssh2 tree.
FetchContent_MakeAvailable(libssh2)

# ---- Core library -----------------------------------------------------------
qt_add_library(nyan_core STATIC
    src/core/AppPaths.cpp
    src/core/AppPaths.h
    src/core/AppSettings.cpp
    src/core/AppSettings.h
    src/core/HostKeyStore.cpp
    src/core/HostKeyStore.h
    src/core/SecretStore.cpp
    src/core/SecretStore.h
    src/core/SessionProfile.cpp
    src/core/SessionProfile.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/SftpTypes.cpp
    src/core/SftpTypes.h
    src/core/SftpWorker.cpp
    src/core/SftpWorker.h
    src/core/SshConnection.cpp
    src/core/SshConnection.h
    src/core/SshTypes.cpp
    src/core/SshTypes.h
    src/core/SshWorker.cpp
    src/core/SshWorker.h
    src/term/TerminalScreen.cpp
    src/term/TerminalScreen.h
)
target_include_directories(nyan_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(nyan_core PUBLIC Qt6::Core Qt6::Gui libssh2::libssh2 OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(nyan_core PUBLIC ws2_32 crypt32)
endif()
target_compile_definitions(nyan_core PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

# ---- UI library -------------------------------------------------------------
qt_add_library(nyan_ui STATIC
    src/term/TerminalWidget.cpp
    src/term/TerminalWidget.h
    src/ui/AuthDialogs.cpp
    src/ui/AuthDialogs.h
    src/ui/ConnectionDialog.cpp
    src/ui/ConnectionDialog.h
    src/ui/Dialogs.cpp
    src/ui/Dialogs.h
    src/ui/FilePanel.cpp
    src/ui/FilePanel.h
    src/ui/Icons.cpp
    src/ui/Icons.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MaterialIcons.cpp
    src/ui/MaterialIcons.h
    src/ui/MaterialWidgets.cpp
    src/ui/MaterialWidgets.h
    src/ui/SessionTab.cpp
    src/ui/SessionTab.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/Sidebar.cpp
    src/ui/Sidebar.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/TitleBar.cpp
    src/ui/TitleBar.h
    src/ui/WelcomePage.cpp
    src/ui/WelcomePage.h
    resources/resources.qrc
)
target_link_libraries(nyan_ui PUBLIC nyan_core Qt6::Widgets Qt6::Svg)
if(WIN32)
    target_link_libraries(nyan_ui PRIVATE dwmapi)
endif()

# ---- Application ------------------------------------------------------------
qt_add_executable(NyanSSH WIN32 src/main.cpp)
target_link_libraries(NyanSSH PRIVATE nyan_ui)
set_target_properties(NyanSSH PROPERTIES OUTPUT_NAME "NyanSSH")
if(WIN32)
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
    configure_file(resources/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
        message(FATAL_ERROR "resources/app.ico is required for the Windows executable icon.")
    endif()
    target_sources(NyanSSH PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()
nyan_copy_openssl_runtime(NyanSSH)

# ---- Backend smoke test -----------------------------------------------------
qt_add_executable(nyan_smoke tests/smoke/main.cpp)
target_link_libraries(nyan_smoke PRIVATE nyan_core)
set_target_properties(nyan_smoke PROPERTIES WIN32_EXECUTABLE OFF)
nyan_copy_openssl_runtime(nyan_smoke)

# ---- Tests ------------------------------------------------------------------
option(NYAN_BUILD_TESTS "Build Nyan SSH unit tests" ON)
enable_testing()
if(NYAN_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(nyan_tests
        tests/unit/test_main.cpp
    )
    target_link_libraries(nyan_tests PRIVATE nyan_core Qt6::Test)
    add_test(NAME core COMMAND nyan_tests)
    nyan_copy_openssl_runtime(nyan_tests)

    qt_add_executable(nyan_ui_probe tests/ui_probe.cpp)
    target_link_libraries(nyan_ui_probe PRIVATE nyan_ui)
    set_target_properties(nyan_ui_probe PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_ui_probe)

    qt_add_executable(nyan_seed tests/seed.cpp)
    target_link_libraries(nyan_seed PRIVATE nyan_core)
    set_target_properties(nyan_seed PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_seed)

    qt_add_executable(nyan_ui_integration tests/ui_integration.cpp)
    target_link_libraries(nyan_ui_integration PRIVATE nyan_ui Qt6::Test)
    set_target_properties(nyan_ui_integration PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_ui_integration)

    qt_add_executable(nyan_dialog_probe tests/dialog_probe.cpp)
    target_link_libraries(nyan_dialog_probe PRIVATE nyan_ui)
    set_target_properties(nyan_dialog_probe PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_dialog_probe)

    qt_add_executable(nyan_sftp_worker_test tests/sftp_worker_test.cpp)
    target_link_libraries(nyan_sftp_worker_test PRIVATE nyan_core)
    set_target_properties(nyan_sftp_worker_test PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_sftp_worker_test)

    qt_add_executable(nyan_ssh_worker_test tests/ssh_worker_test.cpp)
    target_link_libraries(nyan_ssh_worker_test PRIVATE nyan_core)
    set_target_properties(nyan_ssh_worker_test PROPERTIES WIN32_EXECUTABLE OFF)
    nyan_copy_openssl_runtime(nyan_ssh_worker_test)
endif()

# ---- Install / packaging ----------------------------------------------------
include(GNUInstallDirs)
install(TARGETS NyanSSH
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
install(FILES README.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
if(WIN32)
    install(FILES resources/app.ico DESTINATION .)
endif()
install(FILES resources/icons/logo.svg DESTINATION . RENAME app-icon.svg)
if(WIN32 AND NYAN_OPENSSL_SHARED)
    install(FILES "${NYAN_OPENSSL_CRYPTO_DLL}" DESTINATION .)
endif()
install(FILES licenses/Qt-LICENSE DESTINATION licenses)
install(FILES licenses/MaterialIcons-LICENSE.txt DESTINATION licenses)
install(FILES licenses/libssh2-COPYING DESTINATION licenses RENAME libssh2-COPYING)
install(FILES licenses/OpenSSL-LICENSE.txt DESTINATION licenses)
install(DIRECTORY docs DESTINATION . FILES_MATCHING PATTERN "*.png")

if(WIN32)
    # Keep the executable and Qt runtime DLLs side by side in the package root.
    set(QT_DEPLOY_BIN_DIR ".")
    set(QT_DEPLOY_PLUGINS_DIR "plugins")
    qt_generate_deploy_app_script(
        TARGET NyanSSH
        OUTPUT_SCRIPT NYAN_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
        NO_TRANSLATIONS
    )
    install(CODE [=[set(QT_DEPLOY_BIN_DIR ".")]=])
    install(CODE [=[set(QT_DEPLOY_PLUGINS_DIR "plugins")]=])
    install(SCRIPT ${NYAN_DEPLOY_SCRIPT})
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "NyanSSH")
set(CPACK_PACKAGE_VENDOR "Nyan")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "NyanSSH-${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Material-style SSH client")
include(CPack)
