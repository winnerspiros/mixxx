cmake_minimum_required(VERSION 3.21)
# lint_cmake: -readability/wonkycase

message(STATUS "CMAKE_VERSION: ${CMAKE_VERSION}")

# CMAKE_CXX_COMPILER_ID: Distinguish between "AppleClang" and "Clang"
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# MACOSX_RPATH is set by default
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

# Support new IN_LIST if() operator
if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

# Enforce interprocedural optimization
if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()

# Let AUTOMOC and AUTOUIC process GENERATED files
if(POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif()

# Propagate interface link properties
if(POLICY CMP0099)
  # This avoids a warning when qt deals with different behaviours controlled by this policy
  # in its cmake functions. See
  # https://github.com/qt/qtbase/commit/e3e1007f9778d8fc629a06f7d3d216bb7f81351b
  cmake_policy(SET CMP0099 NEW)
endif()

# Add support for cmake_dependent_option
if(POLICY CMP0127)
  cmake_policy(SET CMP0127 NEW)
endif()

# An imported target missing its location property fails during generation.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif()

# Set the timestamp of extracted files to the time of the extraction instead of
# the archived timestamp to make sure that dependent files are rebuilt if the
# URL changes.
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Android)
  if(NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME is not set. Did you source the setup file?")
  endif()
  if((NOT CMAKE_ANDROID_NDK) AND DEFINED ENV{ANDROID_NDK_HOME})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
  endif()
  set(ANDROID ON)
  if(DEFINED ENV{QT_ANDROID_KEYSTORE_PATH})
    set(QT_ANDROID_SIGN_APK ON)
  endif()

  set(QT_ANDROID_APP_PATH "$<TARGET_FILE:${SOURCE_TARGET}>")
  set(
    QT_ANDROID_APP_PACKAGE_SOURCE_ROOT
    "${CMAKE_SOURCE_DIR}/packaging/android"
  )

  if((NOT ANDROID_SDK_ROOT) AND DEFINED ENV{ANDROID_SDK})
    set(ANDROID_SDK_ROOT "$ENV{ANDROID_SDK}")
  endif()
  set(ANDROID_ABI arm64-v8a)
  set(ANDROID_NDK_HOST_SYSTEM_NAME linux-x86_64)
  set(CMAKE_SYSTEM_VERSION 35) # API level
  set(ANDROID_PLATFORM "android-${CMAKE_SYSTEM_VERSION}")
  set(ANDROID_API_VERSION "android-${CMAKE_SYSTEM_VERSION}")
  set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}") # or x86_64, armeabi-v7a, etc.
  set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
  set(CMAKE_ANDROID_STL_TYPE c++_shared)
  set(
    CMAKE_SYSROOT
    "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_SYSTEM_NAME}/sysroot"
  )
  include_directories(
    BEFORE
    SYSTEM
    "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_SYSTEM_NAME}/sysroot/usr/include/"
  )
  set(
    CMAKE_LIBRARY_PATH
    "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_SYSTEM_NAME}/sysroot/usr/lib/aarch64-linux-android/${CMAKE_SYSTEM_VERSION}/;${CMAKE_LIBRARY_PATH}"
  )
endif()

if(
  ((APPLE AND NOT IOS) OR WIN32 OR ANDROID)
  AND NOT IS_DIRECTORY "${MIXXX_VCPKG_ROOT}"
)
  if(NOT DEFINED BUILDENV_BASEPATH)
    if(DEFINED ENV{BUILDENV_BASEPATH})
      set(BUILDENV_BASEPATH "$ENV{BUILDENV_BASEPATH}")
    else()
      set(BUILDENV_BASEPATH "${CMAKE_SOURCE_DIR}/buildenv")
    endif()
  endif()

  if(NOT DEFINED BUILDENV_URL)
    if(DEFINED ENV{BUILDENV_URL})
      set(BUILDENV_URL "$ENV{BUILDENV_URL}")
    elseif(NOT DEFINED BUILDENV_URL)
      message(FATAL_ERROR "BUILDENV_URL not specified")
    endif()
  endif()

  # Extract the filename from the URL
  get_filename_component(FILENAME_WITH_EXTENSION "${BUILDENV_URL}" NAME)

  # Remove the extension from the filename (note that our buildenv names contain two dots)
  string(
    REGEX REPLACE
    "\\.[^.]*$"
    ""
    BUILDENV_NAME
    "${FILENAME_WITH_EXTENSION}"
  )

  message(STATUS "BUILDENV_NAME is: ${BUILDENV_NAME}")

  if(
    NOT EXISTS "${BUILDENV_BASEPATH}/${BUILDENV_NAME}"
    OR NOT IS_DIRECTORY "${BUILDENV_BASEPATH}/${BUILDENV_NAME}"
  )
    if(NOT DEFINED BUILDENV_SHA256)
      if(DEFINED ENV{BUILDENV_SHA256})
        set(BUILDENV_SHA256 "$ENV{BUILDENV_SHA256}")
      elseif(NOT DEFINED BUILDENV_SHA256)
        message(STATUS "BUILDENV_SHA256 not specified")
      endif()
    endif()

    if(NOT EXISTS "${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip")
      message(
        STATUS
        "Downloading file ${BUILDENV_URL} to ${BUILDENV_BASEPATH} ..."
      )
      file(
        DOWNLOAD ${BUILDENV_URL} "${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip"
        SHOW_PROGRESS
        TLS_VERIFY ON
      )
    else()
      # Reference to suppress intentionally unused variable warnings
      set(_dummy "${BUILDENV_URL}")
    endif()

    if(NOT ${BUILDENV_SHA256} STREQUAL "")
      message(
        STATUS
        "Verify SHA256 of downloaded file ${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip ..."
      )
      file(SHA256 "${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip" actual_sha256)

      if(NOT actual_sha256 STREQUAL ${BUILDENV_SHA256})
        message(
          FATAL_ERROR
          "SHA256 checksum mismatch:\nexpected: ${BUILDENV_SHA256}\n     got: ${actual_sha256}"
        )
      else()
        message(STATUS "SHA256 ${BUILDENV_SHA256} is correct!")
      endif()
    endif()

    message(
      STATUS
      "Unpacking file ${BUILDENV_BASEPATH}/${BUILDENV_NAME}.zip ..."
    )
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xzf "${BUILDENV_NAME}.zip"
      WORKING_DIRECTORY ${BUILDENV_BASEPATH}
    )
  else()
    # Reference to suppress intentionally unused variable warnings
    set(_dummy "${BUILDENV_SHA256}")
  endif()
else()
  # Reference to suppress intentionally unused variable warnings
  set(_dummy "${BUILDENV_URL} ${BUILDENV_BASEPATH} ${BUILDENV_SHA256}")
endif()

# Use this function to throw an error because the build environment is not set
# up correctly.
function(fatal_error_missing_env)
  if(WIN32)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      message(
        FATAL_ERROR
        "Did you download the Mixxx build environment using `${CMAKE_SOURCE_DIR}/tools/windows_buildenv.bat`?"
      )
    else()
      message(
        FATAL_ERROR
        "Did you download the Mixxx build environment using `${CMAKE_SOURCE_DIR}/tools/windows_release_buildenv.bat` or `${CMAKE_SOURCE_DIR}/tools/windows_buildenv.bat`(includes Debug)?"
      )
    endif()
  elseif(APPLE AND NOT IOS)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      message(
        FATAL_ERROR
        "Did you build the Mixxx build environment as described here: https://github.com/mixxxdj/mixxx/wiki/Compiling-dependencies-for-macOS-arm64?"
      )
    elseif(CMAKE_BUILD_TYPE MATCHES "Debug")
      message(
        FATAL_ERROR
        "Did you download the Mixxx build environment using `source ${CMAKE_SOURCE_DIR}/tools/macos_buildenv.sh setup`"
      )
    else()
      message(
        FATAL_ERROR
        "Did you download the Mixxx build environment using `source ${CMAKE_SOURCE_DIR}/tools/macos_release_buildenv.sh setup` or `source ${CMAKE_SOURCE_DIR}/tools/macos_buildenv.sh setup` (includes Debug)?"
      )
    endif()
  elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    # Linux, BSD, Solaris, Minix
    if(EXISTS "/etc/debian_version") # exists also on Ubuntu and Mint
      message(
        FATAL_ERROR
        "Did you install the required Debian dev packages via `${CMAKE_SOURCE_DIR}/tools/debian_buildenv.sh setup`?"
      )
    elseif(EXISTS "/etc/redhat-release") # exists also on Fedora Mageia Madndriva Alma CentOS
      message(
        FATAL_ERROR
        "Did you install the required RPM dev packages via `${CMAKE_SOURCE_DIR}/tools/rpm_buildenv.sh setup`?"
      )
    else()
      message(
        FATAL_ERROR
        "Did you install the equivalent dev packages listed in `${CMAKE_SOURCE_DIR}/tools/debian_buildenv.sh setup`?"
      )
    endif()
  elseif(DEFINED VCPKG_TARGET_TRIPLET)
    message(
      FATAL_ERROR
      "You are targeting ${VCPKG_TARGET_TRIPLET}, which does not have a prebuilt environment. Please make sure that -DMIXXX_VCPKG_ROOT points to a vcpkg environment containing installed dependencies for ${VCPKG_TARGET_TRIPLET}!"
    )
  else()
    message(
      FATAL_ERROR
      "You are building for an unknown platform and are missing a build environment. Please set -DVCPKG_TARGET_TRIPLET and make sure that -DMIXXX_VCPKG_ROOT points to a vcpkg environment containing installed dependencies for your target platform!"
    )
  endif()
endfunction()

if(UNIX AND NOT APPLE)
  execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE UNAME_R
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(UNAME_R MATCHES ".*[Mm]icrosoft.*" OR UNAME_R MATCHES ".*WSL.*")
    message(
      FATAL_ERROR
      "Building on Windows Subsystem for Linux (WSL) is not supported. If you want to build Mixxx from command line on Windows, you need to use the \"x64 Native Tools Command Prompt for VS 2022\"!"
    )
  endif()
endif()
if(WIN32)
  # Check if we are running from "Visual Studio 20XX Developer Command Prompt"
  if(NOT DEFINED ENV{INCLUDE})
    message(
      FATAL_ERROR
      "The INCLUDE environment variable is not defined. Please ensure you are using the correct shell like 'x64 Native Tools Command Prompt for VS 20XX'"
    )
  endif()
endif()
if(WIN32)
  # Check if we are running from "Visual Studio 20XX Developer Command Prompt"
  if(NOT DEFINED ENV{INCLUDE})
    message(
      FATAL_ERROR
      "The INCLUDE environment variable is not defined. Please ensure you are using the correct shell like 'x64 Native Tools Command Prompt for VS 20XX'"
    )
  endif()
endif()

# We use here ENV{MIXXX_VCPKG_ROOT} as a workaround to find the overlay folders
# in manifest mode https://github.com/microsoft/vcpkg/issues/12289.
# Note: VCPKG_ROOT, the default location for the vcpkg cli tool is later
# adjusted by CMAKE_TOOLCHAIN_FILE.
if(DEFINED ENV{MIXXX_VCPKG_ROOT} AND NOT DEFINED MIXXX_VCPKG_ROOT)
  set(MIXXX_VCPKG_ROOT "$ENV{MIXXX_VCPKG_ROOT}")
endif()

if(DEFINED MIXXX_VCPKG_ROOT)
  if(
    EXISTS "${MIXXX_VCPKG_ROOT}/overlay/ports"
    OR NOT EXISTS "${MIXXX_VCPKG_ROOT}/ports"
  )
    # MIXXX_VCPKG_ROOT points to our vcpkg environment
    # and we configure the CMAKE_TOOLCHAIN_FILE and overlays accordingly
    message(STATUS "Using MIXXX_VCPKG_ROOT: ${MIXXX_VCPKG_ROOT}")
  else()
    message(
      STATUS
      "MIXXX_VCPKG_ROOT not correct (missing ${MIXXX_VCPKG_ROOT}/overlay/ports)"
    )
    fatal_error_missing_env()
  endif()

  if(NOT DEFINED VCPKG_OVERLAY_PORTS)
    # required for manifest mode
    set(VCPKG_OVERLAY_PORTS "${MIXXX_VCPKG_ROOT}/overlay/ports")
    if(APPLE)
      list(APPEND VCPKG_OVERLAY_PORTS "${MIXXX_VCPKG_ROOT}/overlay/osx")
    elseif(WIN32)
      list(APPEND VCPKG_OVERLAY_PORTS "${MIXXX_VCPKG_ROOT}/overlay/windows")
    endif()
  endif()

  if(NOT DEFINED VCPKG_OVERLAY_TRIPLETS)
    # required for manifest mode
    set(VCPKG_OVERLAY_TRIPLETS "${MIXXX_VCPKG_ROOT}/overlay/triplets")
  endif()

  if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(
      CMAKE_TOOLCHAIN_FILE
      "${MIXXX_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING
      ""
    )
  endif()
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
  if(DEFINED ENV{VCPKG_TARGET_TRIPLET})
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_TARGET_TRIPLET}")
  elseif(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}")
  endif()
endif()
set(
  X_VCPKG_APPLOCAL_DEPS_INSTALL
  ON
  CACHE BOOL
  "Automatically copy dependencies into the install target directory for executables."
  FORCE
)

# Set a default build type if none was specified
# See https://blog.kitware.com/cmake-and-the-default-build-type/ for details.
set(default_build_type "RelWithDebInfo")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT WIN32 AND NOT ANDROID)
  # On Windows and Android, Debug builds are linked to unoptimized libs
  # generating unusable slow Mixxx builds.
  set(default_build_type "Debug")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_BUILD_TYPE)
    message(
      STATUS
      "Setting CMAKE_BUILD_TYPE to '${default_build_type}' as none was specified."
    )
    set(
      CMAKE_BUILD_TYPE
      "${default_build_type}"
      CACHE STRING
      "Choose the type of build."
      FORCE
    )
    # Set the possible values of build type for cmake-gui
    set_property(
      CACHE CMAKE_BUILD_TYPE
      PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo"
    )
  elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo)$")
    message(
      FATAL_ERROR
      "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} is not supported, use one of Debug, Release or RelWithDebInfo."
    )
  endif()
endif()

include(CMakeDependentOption)

option(QT6 "Build with Qt6" ON)


  if(STEM)
  list(APPEND MIXXX_LIB_PRECOMPILED_HEADER src/track/steminfo.h)
  target_sources(
    mixxx-lib
    PRIVATE
      src/sources/soundsourcestem.cpp
      src/track/steminfoimporter.cpp
      src/track/steminfo.cpp
      src/widget/wtrackstemmenu.cpp
      src/widget/wstemlabel.cpp
  )
  if(QOPENGL)
    target_sources(
      mixxx-lib
      PRIVATE src/waveform/renderers/allshader/waveformrendererstem.cpp
    )
  endif()
endif()

if(BUILD_TESTING)
  set(
    src-mixxx-test
    src/test/analyserwaveformtest.cpp
    src/test/analyzersilence_test.cpp
    src/test/audiotaperpot_test.cpp
    src/test/autodjprocessor_test.cpp
    src/test/beatgridtest.cpp
    src/test/beatmaptest.cpp
    src/test/beatstest.cpp
    src/test/beatstranslatetest.cpp
    src/test/borrowabletest.cpp
    src/test/bpmtest.cpp
    src/test/bpmcontrol_test.cpp
    src/test/broadcastprofile_test.cpp
    src/test/broadcastsettings_test.cpp
    src/test/cache_test.cpp
    src/test/channelhandle_test.cpp
    src/test/chrono_clock_resolution_test.cpp
    src/test/colorconfig_test.cpp
    src/test/colormapperjsproxy_test.cpp
    src/test/colorpalette_test.cpp
    src/test/configobject_test.cpp
    src/test/controller_mapping_validation_test.cpp
    src/test/controller_mapping_settings_test.cpp
    src/test/controllers/controller_columnid_regression_test.cpp
    src/test/controllerscriptenginelegacy_test.cpp
    src/test/controlobjecttest.cpp
    src/test/controlobjectaliastest.cpp
    src/test/controlobjectscripttest.cpp
    src/test/controlpotmetertest.cpp
    src/test/coreservicestest.cpp
    src/test/coverartcache_test.cpp
    src/test/coverartutils_test.cpp
    src/test/cratestorage_test.cpp
    src/test/cue_test.cpp
    src/test/cuecontrol_test.cpp
    src/test/dbconnectionpool_test.cpp
    src/test/dbidtest.cpp
    src/test/directorydaotest.cpp
    src/test/duration_test.cpp
    src/test/durationutiltest.cpp
    #TODO: write useful tests for refactored effects system
    #src/test/effectchainslottest.cpp
    src/test/enginebufferscalelineartest.cpp
    src/test/enginebuffertest.cpp
    src/test/enginefilterbiquadtest.cpp
    src/test/enginemixertest.cpp
    src/test/enginemicrophonetest.cpp
    src/test/enginesynctest.cpp
    src/test/fileinfo_test.cpp
    src/test/frametest.cpp
    src/test/globaltrackcache_test.cpp
    src/test/hotcuecontrol_test.cpp
    src/test/hotcueorderbyposition_test.cpp
    src/test/imageutils_test.cpp
    src/test/indexrange_test.cpp
    src/test/itunesxmlimportertest.cpp
    src/test/keyfactorytest.cpp
    src/test/keyutilstest.cpp
    src/test/lcstest.cpp
    src/test/learningutilstest.cpp
    src/test/libraryscannertest.cpp
    src/test/librarytest.cpp
    src/test/looping_control_test.cpp
    src/test/main.cpp
    src/test/mathutiltest.cpp
    src/test/metadatatest.cpp
    #TODO: make this build again
    #src/test/metaknob_link_test.cpp
    src/test/midicontrollertest.cpp
    src/test/mixxxtest.cpp
    src/test/mock_networkaccessmanager.cpp
    src/test/musicbrainzrecordingstasktest.cpp
    src/test/performancetimer_test.cpp
    src/test/playcountertest.cpp
    src/test/playermanagertest.cpp
    src/test/playlisttest.cpp
    src/test/portmidicontroller_test.cpp
    src/test/portmidienumeratortest.cpp
    src/test/queryutiltest.cpp
    src/test/rangelist_test.cpp
    src/test/readaheadmanager_test.cpp
    src/test/replaygaintest.cpp
    src/test/rescalertest.cpp
    src/test/rgbcolor_test.cpp
    src/test/rotary_test.cpp
    src/test/samplebuffertest.cpp
    src/test/schemamanager_test.cpp
    src/test/searchqueryparsertest.cpp
    src/test/seratobeatgridtest.cpp
    src/test/seratomarkerstest.cpp
    src/test/seratomarkers2test.cpp
    src/test/seratotagstest.cpp
    src/test/signalpathtest.cpp
    src/test/skincontext_test.cpp
    src/test/softtakeover_test.cpp
    src/test/soundproxy_test.cpp
    src/test/soundsourceproviderregistrytest.cpp
    src/test/sqliteliketest.cpp
    src/test/synccontroltest.cpp
    src/test/synctrackmetadatatest.cpp
    src/test/tableview_test.cpp
    src/test/taglibtest.cpp
    src/test/trackdao_test.cpp
    src/test/trackexport_test.cpp
    src/test/trackmetadata_test.cpp
    src/test/trackmetadataexport_test.cpp
    src/test/tracknumberstest.cpp
    src/test/trackreftest.cpp
    src/test/trackupdate_test.cpp
    src/test/uuid_test.cpp
    src/test/wbatterytest.cpp
    src/test/wpushbutton_test.cpp
    src/test/wwidgetstack_test.cpp
    src/util/moc_included_test.cpp
    src/test/helpers/log_test.cpp
  )
  if(STEM)
    set(
      src-mixxx-test
      ${src-mixxx-test}
      src/test/stemtest.cpp
      src/test/steminfotest.cpp
      src/test/stemcontrolobjecttest.cpp
    )
  endif()
  if(BUILD_BENCH)
    set(
      src-mixxx-test
      ${src-mixxx-test}
      src/test/engineeffectsdelay_test.cpp
      src/test/movinginterquartilemean_test.cpp
      src/test/nativeeffects_test.cpp
      src/test/ringdelaybuffer_test.cpp
      src/test/sampleutiltest.cpp
      src/test/waveform_upgrade_test.cpp
    )
  endif()

  add_executable(mixxx-test ${src-mixxx-test})
  target_compile_definitions(
    mixxx-test
    PUBLIC RESOURCE_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/res"
  )

  if(HID)
    target_sources(
      mixxx-test
      PRIVATE src/test/controller_hid_reportdescriptor_test.cpp
    )
  endif()

  if(QML)
    target_sources(
      mixxx-test
      PRIVATE
        src/test/controller_mapping_file_handler_test.cpp
        src/test/controllerrenderingengine_test.cpp
        src/test/qmlcontrolproxytest.cpp
    )
  endif()

  if(AU_EFFECTS)
    target_sources(mixxx-test PRIVATE src/test/audiounitmanager_test.mm)
  endif()

  set_target_properties(mixxx-test PROPERTIES AUTOMOC ON)
  target_link_libraries(
    mixxx-test
    PRIVATE mixxx-lib mixxx-gitinfostore GTest::gtest GTest::gmock
  )

  if(BUILD_BENCH)
    add_compile_definitions(USE_BENCH)
    target_link_libraries(mixxx-test PRIVATE benchmark::benchmark)
  endif()

  # Test Suite
  include(CTest)
  include(GoogleTest)
  enable_testing()

  file(
    GLOB_RECURSE CONTROLLER_BULK_MAPPING_LIST
    "${CMAKE_CURRENT_SOURCE_DIR}/res/controllers/*.bulk.xml"
  )
  file(
    GLOB_RECURSE CONTROLLER_HID_MAPPING_LIST
    "${CMAKE_CURRENT_SOURCE_DIR}/res/controllers/*.hid.xml"
  )
  file(
    GLOB_RECURSE CONTROLLER_MIDI_MAPPING_LIST
    "${CMAKE_CURRENT_SOURCE_DIR}/res/controllers/*.midi.xml"
  )
  list(JOIN CONTROLLER_BULK_MAPPING_LIST "\",\"" CONTROLLER_BULK_MAPPINGS)
  list(JOIN CONTROLLER_HID_MAPPING_LIST "\",\"" CONTROLLER_HID_MAPPINGS)
  list(JOIN CONTROLLER_MIDI_MAPPING_LIST "\",\"" CONTROLLER_MIDI_MAPPINGS)

  target_compile_definitions(
    mixxx-test
    PUBLIC CONTROLLER_BULK_MAPPINGS="${CONTROLLER_BULK_MAPPINGS}"
    PUBLIC CONTROLLER_HID_MAPPINGS="${CONTROLLER_HID_MAPPINGS}"
    PUBLIC CONTROLLER_MIDI_MAPPINGS="${CONTROLLER_MIDI_MAPPINGS}"
  )

  gtest_discover_tests(
    mixxx-test
    EXTRA_ARGS --logLevel info
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TEST_LIST testsuite
    DISCOVERY_MODE PRE_TEST
  )

  if(BUILD_BENCH)
    # Benchmarking
    add_custom_target(
      mixxx-benchmark
      COMMAND $<TARGET_FILE:mixxx-test> --benchmark
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Mixxx Benchmarks"
      VERBATIM
    )
    add_dependencies(mixxx-benchmark mixxx-test)
  endif()
endif() # BUILD_TESTING

#
# Resources
#
# Add resources to mixxx and mixxx-test binaries, not the mixxx-lib static
# library. Doing this would require initialization using Q_INIT_RESOURCE()
# calls that are not present at the moment. Further information can be found
# at: https://doc.qt.io/qt5/resources.html#using-resources-in-a-library
option(DOWNLOAD_MANUAL "Download Manual PDF from Mixxx website" OFF)
if(
  DOWNLOAD_MANUAL
  AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/Mixxx-Manual.pdf"
)
  set(
    MANUAL_URL
    "https://downloads.mixxx.org/manual/${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}/mixxx-manual-${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}-en.pdf"
  )
  message(STATUS "Downloading manual from ${MANUAL_URL}...")
  file(
    DOWNLOAD "${MANUAL_URL}" "${CMAKE_CURRENT_BINARY_DIR}/res/Mixxx-Manual.pdf"
    SHOW_PROGRESS
    STATUS MANUAL_PDF_DOWNLOAD
    TLS_VERIFY ON
  )
  list(GET MANUAL_PDF_DOWNLOAD 0 MANUAL_PDF_DOWNLOAD_ERROR)
  if(NOT MANUAL_PDF_DOWNLOAD_ERROR EQUAL 0)
    list(GET MANUAL_PDF_DOWNLOAD 1 MANUAL_PDF_DOWNLOAD_MESSGAE)
    message(
      FATAL_ERROR
      "Manual PDF download failed with: "
      "${MANUAL_PDF_DOWNLOAD_MESSGAE} Code: ${MANUAL_PDF_DOWNLOAD_ERROR}. "
      "Either download it yourself and move it to "
      "'${CMAKE_CURRENT_SOURCE_DIR}/res/Mixxx-Manual.pdf' or "
      "reconfigure with -DDOWNLOAD_MANUAL=OFF to build without included "
      "manual."
    )
  endif()
  file(
    RENAME
    "${CMAKE_CURRENT_BINARY_DIR}/res/Mixxx-Manual.pdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/Mixxx-Manual.pdf"
  )
endif()

target_sources(mixxx PRIVATE res/mixxx.qrc)
set_target_properties(mixxx PROPERTIES AUTORCC ON)
if(BUILD_TESTING)
  target_sources(mixxx-test PRIVATE res/mixxx.qrc)
  set_target_properties(mixxx-test PROPERTIES AUTORCC ON)
endif()

if(MIXXX_VERSION_PRERELEASE STREQUAL "")
  set(MIXXX_VERSION "${CMAKE_PROJECT_VERSION}")
else()
  set(MIXXX_VERSION "${CMAKE_PROJECT_VERSION}-${MIXXX_VERSION_PRERELEASE}")
endif()

get_target_property(MIXXX_BUILD_FLAGS mixxx-lib COMPILE_OPTIONS)

# uses CMAKE_PROJECT_VERSION MIXXX_VERSION_PRERELEASE MIXXX_BUILD_FLAGS
configure_file(src/version.h.in src/version.h @ONLY)

if(
  GIT_COMMIT_DATE
  AND
    NOT
      GIT_COMMIT_DATE
        MATCHES
        "^[0-9]*-[0-9]*-[0-9]*T[0-9]*\\:[0-9]*\\:[0-9]*[+-][0-9]*\\:[0-9]*$"
)
  message(
    FATAL_ERROR
    "GIT_COMMIT_DATE requires strict ISO 8601 format %Y-%m-%dT%H:%M:%SZ"
  )
endif()

add_custom_target(
  mixxx-gitinfo
  # Note: We don't quote the paths in the command since CMake already inserts
  # escapes (which, if quoted, lead to paths wrongly containing backslashes).
  # See https://stackoverflow.com/questions/8925396/why-does-cmake-prefixes-spaces-with-backslashes-when-executing-a-command
  COMMAND
    ${CMAKE_COMMAND} -DGIT_DESCRIBE=${GIT_DESCRIBE}
    -DGIT_COMMIT_DATE=${GIT_COMMIT_DATE}
    -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/gitinfo.h.in
    -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/src/gitinfo.h -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/gitinfo.cmake
  COMMENT "Update git version information in gitinfo.h"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/src/gitinfo.h"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Windows-only resource file
if(WIN32)
  string(TIMESTAMP MIXXX_YEAR "%Y")

  set(
    MIXXX_FILEVERSION
    "${CMAKE_PROJECT_VERSION_MAJOR},${CMAKE_PROJECT_VERSION_MINOR},${CMAKE_PROJECT_VERSION_PATCH}"
  )
  set(MIXXX_PRODUCTVERSION "${MIXXX_FILEVERSION}")

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MIXXX_DEBUG 1)
  else()
    set(MIXXX_DEBUG 0)
  endif()

  if(MIXXX_VERSION_PRERELEASE STREQUAL "")
    set(MIXXX_PRERELEASE 0)
  else()
    set(MIXXX_PRERELEASE 1)
  endif()

  # uses MIXXX_YEAR MIXXX_FILEVERSION MIXXX_PRODUCTVERSION MIXXX_VERSION MIXXX_DEBUG MIXXX_PRERELEASE
  configure_file("src/mixxx.rc.include.in" "src/mixxx.rc.include" @ONLY)
  add_dependencies(mixxx mixxx-gitinfo)

  target_sources(
    mixxx
    PRIVATE
      src/mixxx.rc
      "${CMAKE_CURRENT_BINARY_DIR}/src/mixxx.rc.include"
      "${CMAKE_CURRENT_BINARY_DIR}/src/gitinfo.h"
  )
  target_include_directories(mixxx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Chromaprint
find_package(Chromaprint)
if(NOT Chromaprint_FOUND)
  # Fail verbose, because with Qt Creator, the verbose error message for Qt is bypassed.
  fatal_error_missing_env()
endif()
target_link_libraries(mixxx-lib PRIVATE Chromaprint::Chromaprint)

# Locale Aware Compare for SQLite
find_package(SQLite3)
# For LOCALECOMPARE we call directly sqlite functions to the database opened by
# Qt. It only works without crashing when Mixxx links to the same sqlite
# library as Qt.
# This is difficult on macOS where system the SQLite can be installed and linked
# dynamically from various locations. There is no issue in case of static
# linking which would result in a duplicate symbol error.
if(NOT SQLite3_FOUND)
  set(LOCALECOMPARE_DEFAULT OFF)
else()
  is_static_library(SQLite3_IS_STATIC SQLite::SQLite3)
  if(SQLite3_IS_STATIC OR NOT APPLE)
    set(LOCALECOMPARE_DEFAULT ON)
  else()
    set(LOCALECOMPARE_DEFAULT OFF)
  endif()
endif()
cmake_dependent_option(
  LOCALECOMPARE
  "Locale Aware Compare support for SQLite"
  ON
  "LOCALECOMPARE_DEFAULT"
  OFF
)
if(LOCALECOMPARE)
  if(NOT SQLite3_FOUND)
    message(
      FATAL_ERROR
      "Locale Aware Compare for SQLite requires libsqlite and its development headers."
    )
  endif()
  target_compile_definitions(mixxx-lib PUBLIC __SQLITE3__)
  target_link_libraries(mixxx-lib PRIVATE SQLite::SQLite3)
elseif(SQLite3_IS_STATIC)
  # in the static case we need to link SQLite3 uncoditionally
  target_link_libraries(mixxx-lib PRIVATE SQLite::SQLite3)
endif()

# Denon Engine Prime library export support (using libdjinterop)
option(ENGINEPRIME "Support for library export to Denon Engine Prime" ON)
if(ENGINEPRIME)
  # libdjinterop does not currently have a stable ABI, so we fetch sources for a specific tag, build here, and link
  # statically.  This situation should be reviewed once libdjinterop hits version 1.x.
  set(LIBDJINTEROP_VERSION 0.27.0)
  # Look whether an existing installation of libdjinterop matches the required version.
  find_package(DjInterop ${LIBDJINTEROP_VERSION} EXACT CONFIG)
  if(NOT DjInterop_FOUND)
    find_package(DjInterop ${LIBDJINTEROP_VERSION} EXACT MODULE)
  endif()

  if(DjInterop_FOUND)
    # An existing installation of djinterop is available.
    message(STATUS "STATIC link existing system installation of libdjinterop")
    target_link_libraries(mixxx-lib PUBLIC DjInterop::DjInterop)
  else()
    # On MacOS, Mixxx does not use system SQLite, so we will use libdjinterop's
    # embedded SQLite in such a case.
    if(APPLE AND NOT SQLite3_IS_STATIC)
      message(
        STATUS
        "Building libdjinterop sources (with embedded SQLite) fetched from GitHub"
      )
      set(DJINTEROP_SYSTEM_SQLITE OFF)
    else()
      message(
        STATUS
        "Building libdjinterop sources (with system SQLite) fetched from GitHub"
      )
      set(DJINTEROP_SYSTEM_SQLITE ON)
    endif()

    set(
      DJINTEROP_INSTALL_DIR
      "${CMAKE_CURRENT_BINARY_DIR}/lib/libdjinterop-install"
    )
    set(
      DJINTEROP_LIBRARY
      "lib/${CMAKE_STATIC_LIBRARY_PREFIX}djinterop${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    # CMake does not pass lists of paths properly to external projects.
    # This is worked around by changing the list separator.
    string(
      REPLACE
      ";"
      "|"
      PIPE_DELIMITED_CMAKE_PREFIX_PATH
      "${CMAKE_PREFIX_PATH}"
    )

    # For offline builds download the archive file from the URL and
    # copy it into DOWNLOAD_DIR under DOWNLOAD_NAME prior to starting
    # the configuration.
    #
    # If you want to test (locally) an experimental fork/branch of libdjinterop,
    # you can comment out URL and URL_HASH and use GIT_REPOSITORY instead:
    #
    # GIT_REPOSITORY "https://github.com/abcd/your-fork-of-libdjinterop"
    # GIT_TAG "origin/name-of-your-branch"
    #
    ExternalProject_Add(
      libdjinterop
      URL
        "https://github.com/xsco/libdjinterop/archive/refs/tags/${LIBDJINTEROP_VERSION}.tar.gz"
        "https://launchpad.net/~xsco/+archive/ubuntu/djinterop/+sourcefiles/libdjinterop/${LIBDJINTEROP_VERSION}-0ubuntu1/libdjinterop_${LIBDJINTEROP_VERSION}.orig.tar.gz"
      URL_HASH
        SHA256=c4e73bf3907fd45be1c9767bcd9f367cbb7c279b4fe047bf2216bc92ae3d1668
      DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/downloads"
      DOWNLOAD_NAME "libdjinterop-${LIBDJINTEROP_VERSION}.tar.gz"
      PREFIX "libdjinterop-${LIBDJINTEROP_VERSION}"
      INSTALL_DIR ${DJINTEROP_INSTALL_DIR}
      LIST_SEPARATOR "|"
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DCMAKE_PREFIX_PATH=${PIPE_DELIMITED_CMAKE_PREFIX_PATH}
        -DCMAKE_INSTALL_LIBDIR:PATH=lib
        -DCMAKE_MODULE_PATH:PATH=${CMAKE_MODULE_PATH}
        -$<IF:$<BOOL:${CMAKE_TOOLCHAIN_FILE}>,D,U>CMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}
        -$<IF:$<BOOL:${CMAKE_OSX_DEPLOYMENT_TARGET}>,D,U>CMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -$<IF:$<BOOL:${CMAKE_OSX_ARCHITECTURES}>,D,U>CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DSYSTEM_SQLITE=${DJINTEROP_SYSTEM_SQLITE}
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --target DjInterop
      BUILD_BYPRODUCTS <INSTALL_DIR>/${DJINTEROP_LIBRARY}
      EXCLUDE_FROM_ALL TRUE
    )

    # Since we have built libdjinterop from sources as a static library, its
    # transitive dependencies are not automatically recognised.  libdjinterop
    # depends on zlib and optionally sqlite3.  If libdjinterop was configured
    # to depend on system SQLite, Mixxx will already have the dependency.
    # But it does not have zlib, so we explicitly add that here.
    find_package(ZLIB 1.2.8 REQUIRED)
    # The include folder needs to already exist, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
    file(MAKE_DIRECTORY "${DJINTEROP_INSTALL_DIR}/include")

    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
      # Required for 'CMakeFiles/mixxx-lib_autogen_timestamp_deps'
      # This tells 'make" that the libdjinterop is required for the djinterop library
      # BUILD_BYPRODUCTS work for Ninja only, while this workaround does not work for Ninja on macOS
      add_custom_command(
        OUTPUT "${DJINTEROP_INSTALL_DIR}/${DJINTEROP_LIBRARY}"
        DEPENDS libdjinterop
        COMMAND echo libdjinterop installed
        COMMENT
          "Tell 'make' that libdjinterop is required for the djinterop target"
      )
    endif()

    # Assemble a library based on the external project.
    add_library(mixxx-libdjinterop STATIC IMPORTED)
    set_target_properties(
      mixxx-libdjinterop
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DJINTEROP_INSTALL_DIR}/include"
        INTERFACE_LINK_LIBRARIES ZLIB::ZLIB
        IMPORTED_LOCATION "${DJINTEROP_INSTALL_DIR}/${DJINTEROP_LIBRARY}"
    )
    add_dependencies(mixxx-libdjinterop libdjinterop)
    # Add the local include directory explicitly before linking the library
    # to make sure not the system provided headers are used
    target_include_directories(
      mixxx-lib
      BEFORE
      PRIVATE "${DJINTEROP_INSTALL_DIR}/include"
    )
    target_link_libraries(mixxx-lib PRIVATE mixxx-libdjinterop)
  endif()

  # Include conditional sources only required with Engine Prime export support.
  target_sources(
    mixxx-lib
    PRIVATE
      src/library/export/dlglibraryexport.cpp
      src/library/export/engineprimeexportjob.cpp
      src/library/export/libraryexporter.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __ENGINEPRIME__)
endif()

# Ebur128
find_package(Ebur128 REQUIRED)
target_link_libraries(mixxx-lib PRIVATE Ebur128::Ebur128)

# FidLib
add_library(fidlib STATIC EXCLUDE_FROM_ALL lib/fidlib/fidlib.c)
if(MSVC)
  target_compile_definitions(fidlib PRIVATE T_MSVC)
  target_compile_definitions(fidlib PRIVATE _USE_MATH_DEFINES)
  target_compile_options(fidlib PRIVATE /W3)
elseif(MINGW)
  target_compile_definitions(fidlib PRIVATE T_MINGW)
  target_compile_options(
    fidlib
    PRIVATE
      -fno-finite-math-only
      -Wall
      -Wextra
      -Wfloat-conversion
      -Werror=return-type
  )
else()
  target_compile_definitions(fidlib PRIVATE T_LINUX)
  target_compile_options(
    fidlib
    PRIVATE
      -fno-finite-math-only
      -Wall
      -Wextra
      -Wfloat-conversion
      -Werror=return-type
  )
endif()
target_include_directories(mixxx-lib SYSTEM PUBLIC lib/fidlib)
target_link_libraries(mixxx-lib PRIVATE fidlib)

# Create a list from CMAKE_PREFIX_PATH with an alternate separator
# This is required to forward CMAKE_PREFIX_PATH in ExternalProject_Add()
# using the LIST_SEPARATOR option
string(REPLACE ";" "|" CMAKE_PREFIX_PATH_ALT_SEP "${CMAKE_PREFIX_PATH}")

# KeyFinder
option(KEYFINDER "KeyFinder support" ON)
if(KEYFINDER)
  set(MIN_LIBKEYFINDER_VERSION 2.2.4)
  set(FETCH_LIBKEYFINDER_VERSION 2.2.8)
  find_package(KeyFinder ${MIN_LIBKEYFINDER_VERSION})
  if(KeyFinder_FOUND)
    target_link_libraries(mixxx-lib PRIVATE KeyFinder::KeyFinder)
  else()
    # If KeyFinder is built statically, we need FFTW
    find_package(FFTW3 REQUIRED)
    set(
      KeyFinder_INSTALL_DIR
      "${CMAKE_CURRENT_BINARY_DIR}/lib/keyfinder-install"
    )
    set(
      KeyFinder_LIBRARY
      "${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}keyfinder${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    # CMake does not pass lists of paths properly to external projects.
    # This is worked around by changing the list separator.
    string(
      REPLACE
      ";"
      "|"
      PIPE_DELIMITED_CMAKE_PREFIX_PATH
      "${CMAKE_PREFIX_PATH}"
    )

    # For offline builds download the archive file from the URL and
    # copy it into DOWNLOAD_DIR under DOWNLOAD_NAME prior to starting
    # the configuration.
    ExternalProject_Add(
      libkeyfinder
      URL
        "https://github.com/mixxxdj/libkeyfinder/archive/refs/tags/${FETCH_LIBKEYFINDER_VERSION}.zip"
      URL_HASH
        SHA256=4f10e9e5673d948776e47e78273fa4d61408155cb0e210af1538c83222f285d4
      DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/downloads"
      DOWNLOAD_NAME "libkeyfinder-${FETCH_LIBKEYFINDER_VERSION}.zip"
      PREFIX "libdjinterop-${FETCH_LIBKEYFINDER_VERSION}"
      INSTALL_DIR "${KeyFinder_INSTALL_DIR}"
      LIST_SEPARATOR "|"
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DCMAKE_PREFIX_PATH=${PIPE_DELIMITED_CMAKE_PREFIX_PATH}
        -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
        -$<IF:$<BOOL:${CMAKE_TOOLCHAIN_FILE}>,D,U>CMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}
        -$<IF:$<BOOL:${CMAKE_OSX_DEPLOYMENT_TARGET}>,D,U>CMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -$<IF:$<BOOL:${CMAKE_OSX_ARCHITECTURES}>,D,U>CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DBUILD_TESTING=OFF
      BUILD_COMMAND ${CMAKE_COMMAND} --build .
      BUILD_BYPRODUCTS <INSTALL_DIR>/${KeyFinder_LIBRARY}
      EXCLUDE_FROM_ALL TRUE
    )

    # This is a bit of a hack to make sure that the include directory actually
    # exists when configuring the build.
    # ExternalProject_Add() will create it
    # at compile time, but CMake already
    # checks that all directories passed to
    # target_include_directories() exist
    # during configuration and will throw
    # an error if not.
    file(MAKE_DIRECTORY "${KeyFinder_INSTALL_DIR}/include")

    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
      # Required for 'CMakeFiles/mixxx-lib_autogen_timestamp_deps'
      # This tells 'make" that the libkeyfinder is required for the mixxx-keyfinder library
      # BUILD_BYPRODUCTS work for Ninja only, while this workaround does not work for Ninja on macOS
      add_custom_command(
        OUTPUT "${KeyFinder_INSTALL_DIR}/${KeyFinder_LIBRARY}"
        DEPENDS libkeyfinder
        COMMAND echo libkeyfinder installed
        COMMENT
          "Tell 'make' that libkeyfinder is required for the mixxx-keyfinder target"
      )
    endif()

    add_library(mixxx-keyfinder STATIC IMPORTED)
    add_dependencies(mixxx-keyfinder libkeyfinder)
    set_target_properties(
      mixxx-keyfinder
      PROPERTIES
        IMPORTED_LOCATION "${KeyFinder_INSTALL_DIR}/${KeyFinder_LIBRARY}"
    )
    target_link_libraries(mixxx-keyfinder INTERFACE FFTW3::fftw3)
    target_include_directories(
      mixxx-keyfinder
      INTERFACE "${KeyFinder_INSTALL_DIR}/include"
    )
    target_link_libraries(mixxx-lib PRIVATE mixxx-keyfinder)
  endif()

  target_sources(mixxx-lib PRIVATE src/analyzer/plugins/analyzerkeyfinder.cpp)
  target_compile_definitions(mixxx-lib PUBLIC __KEYFINDER__)
endif()

# FLAC
find_package(FLAC REQUIRED)
target_link_libraries(mixxx-lib PRIVATE FLAC::FLAC)

# FpClassify This is a wrapper around the fpclassify function that prevents
# inlining It is compiled without optimization and allows to use these function
# from -ffast-math optimized objects. The MSVC option /fp:fast does not suffer this issue
add_library(FpClassify STATIC EXCLUDE_FROM_ALL src/util/fpclassify.cpp)
# With gcc >= 14 and -flto some code is inlined to fast-math code and then
# optimized away. Disable it explicit for all compiler just in case.
# Note: This does not set -fno-lto to override an external provided -flto=auto like on
# Launchpad. Setting it explicit below.
set_target_properties(FpClassify PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

if(
  CMAKE_CXX_COMPILER_ID MATCHES "Clang"
  AND CMAKE_CXX_SIMULATE_ID MATCHES "MSVC"
)
  target_compile_options(FpClassify PRIVATE /fp:precise)
elseif(GNU_GCC OR LLVM_CLANG)
  # The option `-ffp-contract=on` must precede `-fno-fast-math`
  # to silence a warning on Clang 14
  target_compile_options(
    FpClassify
    PRIVATE -ffp-contract=on -fno-fast-math -fno-lto
  )
endif()
target_link_libraries(mixxx-lib PRIVATE FpClassify)

# LAME
find_package(mp3lame REQUIRED)
target_link_libraries(mixxx-lib PRIVATE mp3lame::mp3lame)

add_library(
  rekordbox_metadata
  STATIC
  EXCLUDE_FROM_ALL
  lib/rekordbox-metadata/rekordbox_pdb.cpp
  lib/rekordbox-metadata/rekordbox_anlz.cpp
)
target_include_directories(
  rekordbox_metadata
  SYSTEM
  PUBLIC lib/rekordbox-metadata
)
target_link_libraries(mixxx-lib PRIVATE rekordbox_metadata)

#silence "enumeration values not handled in switch" in generated code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(rekordbox_metadata PRIVATE -Wno-switch)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(rekordbox_metadata PRIVATE /w44063)
endif()

# Kaitai for reading Rekordbox libraries
add_library(Kaitai STATIC EXCLUDE_FROM_ALL lib/kaitai/kaitai/kaitaistream.cpp)
target_include_directories(Kaitai SYSTEM PUBLIC lib/kaitai)
target_compile_definitions(Kaitai PRIVATE KS_STR_ENCODING_NONE)
target_link_libraries(rekordbox_metadata PRIVATE Kaitai)
target_link_libraries(mixxx-lib PRIVATE Kaitai)

# For determining MP3 timing offset cases in Rekordbox library feature
add_library(
  MP3GuessEnc
  STATIC
  EXCLUDE_FROM_ALL
  lib/mp3guessenc-0.27.4/mp3guessenc.c
  lib/mp3guessenc-0.27.4/tags.c
  lib/mp3guessenc-0.27.4/decode.c
  lib/mp3guessenc-0.27.4/bit_utils.c
)
if(WIN32)
  target_compile_definitions(
    MP3GuessEnc
    PRIVATE __WINDOWS__ _CRT_SECURE_NO_WARNINGS
  )
endif()
target_include_directories(MP3GuessEnc SYSTEM PUBLIC lib/mp3guessenc-0.27.4)
target_link_libraries(mixxx-lib PRIVATE MP3GuessEnc)

# OpenGL
option(QGLES2 "Use GLES 2.0 (or higher) instead of full OpenGL for UNIX" OFF)
if(IOS)
  target_link_libraries(mixxx-lib PRIVATE "-framework OpenGLES")
  target_compile_definitions(mixxx-lib PUBLIC QT_OPENGL_ES_2)
else()
  if(APPLE)
    # Prefer the system-provided OpenGL framework on macOS to avoid accidentally
    # linking some other implementation, e.g. /usr/X11R6/lib/libGL.dylib if the
    # user has XQuartz installed, due to vcpkg setting CMAKE_FIND_FRAMEWORK to
    # LAST (see https://cmake.org/cmake/help/latest/variable/CMAKE_FIND_FRAMEWORK.html)
    set(_previous_find_framework ${CMAKE_FIND_FRAMEWORK})
    set(CMAKE_FIND_FRAMEWORK FIRST)
  endif()
  set(OpenGL_GL_PREFERENCE "GLVND")
  if(EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    # Emscripten's FindOpenGL.cmake does not create OpenGL::GL
    target_link_libraries(mixxx-lib PRIVATE ${OPENGL_gl_LIBRARY})
    target_compile_definitions(mixxx-lib PUBLIC QT_OPENGL_ES_2)
    # Require WebGL 2.0 (for a WebGL-friendly subset of OpenGL ES 3.0) and
    # enable full OpenGL ES 2.0 emulation as per
    # https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html
    target_link_options(
      mixxx-lib
      PUBLIC -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES2=1
    )
  elseif(ANDROID)
    find_library(GLESv2_LIBRARY GLESv2)
    target_link_libraries(mixxx-lib PRIVATE "${GLESv2_LIBRARY}")
    target_compile_definitions(mixxx-lib PUBLIC QT_OPENGL_ES_2)
  else()
    find_package(WrapOpenGL REQUIRED)
    if(OPENGL_opengl_LIBRARY)
      target_link_libraries(mixxx-lib PRIVATE OpenGL::OpenGL)
    else()
      target_link_libraries(mixxx-lib PRIVATE OpenGL::GL)
    endif()
  endif()
  if(UNIX AND QGLES2)
    target_compile_definitions(mixxx-lib PUBLIC QT_OPENGL_ES_2)
  endif()
  if(APPLE)
    # Restore the previous CMAKE_FIND_FRAMEWORK value
    set(CMAKE_FIND_FRAMEWORK ${_previous_find_framework})
  endif()
endif()

# Ogg
find_package(Ogg REQUIRED)
target_link_libraries(mixxx-lib PRIVATE Ogg::ogg)

# Vorbis
find_package(Vorbis REQUIRED)
target_link_libraries(
  mixxx-lib
  PRIVATE Vorbis::vorbis Vorbis::vorbisenc Vorbis::vorbisfile
)

# PortAudio
find_package(PortAudio REQUIRED)
target_link_libraries(mixxx-lib PUBLIC PortAudio::PortAudio)

if(ANDROID)
  target_link_libraries(mixxx-lib PUBLIC OpenSLES android)
  target_compile_definitions(mixxx-lib PUBLIC PA_USE_OBOE)
endif()

# PortAudio Ring Buffer
add_library(
  PortAudioRingBuffer
  STATIC
  EXCLUDE_FROM_ALL
  lib/portaudio/pa_ringbuffer.c
)
target_include_directories(mixxx-lib SYSTEM PUBLIC lib/portaudio)
target_link_libraries(mixxx-lib PRIVATE PortAudioRingBuffer)

# PortMidi
default_option(PORTMIDI "Enable the PortMidi backend for MIDI controllers" "NOT ANDROID")
if(PORTMIDI)
  target_compile_definitions(mixxx-lib PUBLIC __PORTMIDI__)
  find_package(PortMidi REQUIRED)
  target_include_directories(mixxx-lib SYSTEM PUBLIC ${PortMidi_INCLUDE_DIRS})
  target_link_libraries(mixxx-lib PRIVATE ${PortMidi_LIBRARIES})
  target_sources(
    mixxx-lib
    PRIVATE
      src/controllers/midi/portmidicontroller.cpp
      src/controllers/midi/portmidienumerator.cpp
  )
endif()

# Protobuf
add_subdirectory(src/proto)
target_link_libraries(mixxx-lib PUBLIC mixxx-proto)

# Rigtorp SPSC Queue
# https://github.com/rigtorp/SPSCQueue
target_include_directories(
  mixxx-lib
  SYSTEM
  PUBLIC lib/rigtorp/SPSCQueue/include
)

# Qt
set(
  QT_COMPONENTS
  Concurrent
  Core
  Gui
  Network
  OpenGL
  PrintSupport
  Qml # for QJSEngine
  Sql
  Svg
  Test
  Widgets
  Xml
)
set(QT_EXTRA_COMPONENTS "")
if(QT6)
  find_package(QT 6.2 NAMES Qt6 COMPONENTS Core REQUIRED)
  list(APPEND QT_EXTRA_COMPONENTS "ShaderTools")
  list(APPEND QT_EXTRA_COMPONENTS "SvgWidgets")
  list(APPEND QT_EXTRA_COMPONENTS "Core5Compat")
  if(QT_VERSION VERSION_GREATER_EQUAL 6.10)
    # from Qt 6.10 GuiPrivate required for QShader/rendergraph (rhi/qshader.h)
    list(APPEND QT_EXTRA_COMPONENTS "GuiPrivate")
  endif()
else()
  find_package(QT 5.12 NAMES Qt5 COMPONENTS Core REQUIRED)
endif()
if(QML)
  list(APPEND QT_EXTRA_COMPONENTS "LabsQmlModels")
  list(APPEND QT_EXTRA_COMPONENTS "Multimedia")
  list(APPEND QT_EXTRA_COMPONENTS "Quick")
  list(APPEND QT_EXTRA_COMPONENTS "QuickControls2")
  list(APPEND QT_EXTRA_COMPONENTS "QuickControls2Impl")
  list(APPEND QT_EXTRA_COMPONENTS "QuickLayouts")
  list(APPEND QT_EXTRA_COMPONENTS "QuickShapesPrivate")
  list(APPEND QT_EXTRA_COMPONENTS "QuickTemplates2")
  list(APPEND QT_EXTRA_COMPONENTS "QuickWidgets")
  list(APPEND QT_EXTRA_COMPONENTS "ShaderTools")
  if(QT_VERSION VERSION_GREATER_EQUAL 6.7)
    list(APPEND QT_EXTRA_COMPONENTS "QuickControls2Basic")
    list(APPEND QT_EXTRA_COMPONENTS "QuickControls2BasicStyleImpl")
    list(APPEND QT_EXTRA_COMPONENTS "QuickControls2Fusion")
    list(APPEND QT_EXTRA_COMPONENTS "QuickControls2FusionStyleImpl")
  endif()
  if(QT_VERSION VERSION_LESS 6.8)
    # From Qt 6.8 integrated in Qml via QmlMeta
    list(APPEND QT_EXTRA_COMPONENTS "QmlWorkerScript")
  endif()
endif()
if(ANDROID)
  list(REMOVE_ITEM QT_COMPONENTS DBus PrintSupport Multimedia MultimediaWidgets)
  list(REMOVE_ITEM QT_EXTRA_COMPONENTS DBus PrintSupport Multimedia MultimediaWidgets)
endif()
find_package(
  Qt${QT_VERSION_MAJOR}
  COMPONENTS ${QT_COMPONENTS} ${QT_EXTRA_COMPONENTS}
  REQUIRED
)
# PUBLIC is required below to find included headers
foreach(component ${QT_COMPONENTS})
  target_link_libraries(mixxx-lib PUBLIC Qt${QT_VERSION_MAJOR}::${component})
endforeach()
if(QT_EXTRA_COMPONENTS)
  foreach(component ${QT_EXTRA_COMPONENTS})
    target_link_libraries(mixxx-lib PUBLIC Qt${QT_VERSION_MAJOR}::${component})
  endforeach()
endif()

if(QT_KNOWN_POLICY_QTP0002 AND ANDROID)
  qt6_policy(SET QTP0002 NEW)
endif()

if(QML)
  if(QT_KNOWN_POLICY_QTP0004)
    # See: https://doc.qt.io/qt-6/qt-cmake-policy-qtp0004.html
    # OLD (Qt < 6.8) requires to import qml modules with a folder e.g.:
    # fragmentShader: "qrc:/shaders/rgbsignal_qml.frag.qsb"
    # For using NEW, we need to back-port:
    # https://github.com/qt/qtdeclarative/commit/6314d305ee0d9064ca848980ef2dab1793c191b8
    # until Qt 6.8 lands in all supported distros
    qt6_policy(SET QTP0004 OLD)
  endif()

  add_subdirectory(res/shaders)

  set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
  qt_add_library(mixxx-qml-lib STATIC)

  if(WIN32)
    target_compile_definitions(mixxx-qml-lib PUBLIC __WINDOWS__)
  endif()

  if(ENGINEPRIME)
    target_compile_definitions(mixxx-qml-lib PUBLIC __ENGINEPRIME__)
  endif()

  foreach(component ${QT_COMPONENTS})
    target_link_libraries(
      mixxx-qml-lib
      PUBLIC Qt${QT_VERSION_MAJOR}::${component}
    )
  endforeach()
  if(QT_EXTRA_COMPONENTS)
    foreach(component ${QT_EXTRA_COMPONENTS})
      target_link_libraries(
        mixxx-qml-lib
        PUBLIC Qt${QT_VERSION_MAJOR}::${component}
      )
    endforeach()
  endif()
  set_target_properties(mixxx-qml-lib PROPERTIES AUTOMOC ON)
  qt_add_qml_module(mixxx-qml-lib
    URI Mixxx
    VERSION 1.0
    RESOURCE_PREFIX /mixxx.org/imports
    IMPORTS QtQuick
    QML_FILES
      res/qml/Mixxx/MathUtils.mjs
      res/qml/Mixxx/PlayerDropArea.qml
  )
  target_link_libraries(mixxx-lib PRIVATE mixxx-qml-lib)

  # FIXME: Currently we need to add these include directories due to
  # QTBUG-87221. We should figure out a better way to fix this.
  # See: https://bugreports.qt.io/browse/QTBUG-87221
  target_include_directories(mixxx-qml-lib PRIVATE src/control src/qml)
  target_include_directories(mixxx-qml-lib PUBLIC src/ ${CMAKE_BINARY_DIR}/src)
  target_include_directories(
    mixxx-qml-lib
    SYSTEM
    PUBLIC lib/rigtorp/SPSCQueue/include lib/portaudio
  )

  target_link_libraries(mixxx-qml-lib PUBLIC mixxx-proto)
  target_link_libraries(mixxx-qml-libplugin PUBLIC mixxx-proto)

  target_precompile_headers(
    mixxx-qml-lib
    PUBLIC ${MIXXX_COMMON_PRECOMPILED_HEADER}
  )

  target_link_libraries(mixxx-qml-lib PRIVATE mixxx-qml-libplugin)

  qt_add_library(mixxx-qml-mixxxcontrols STATIC)
  set_target_properties(mixxx-qml-mixxxcontrols PROPERTIES AUTOMOC ON)
  qt_add_qml_module(mixxx-qml-mixxxcontrols
    URI Mixxx.Controls
    VERSION 1.0
    RESOURCE_PREFIX /mixxx.org/imports
    OPTIONAL_IMPORTS Mixxx
    QML_FILES
      res/qml/Mixxx/Controls/Knob.qml
      res/qml/Mixxx/Controls/Fader.qml
      res/qml/Mixxx/Controls/Spinny.qml
      res/qml/Mixxx/Controls/WaveformOverviewHotcueMarker.qml
      res/qml/Mixxx/Controls/WaveformOverviewMarker.qml
      res/qml/Mixxx/Controls/WaveformOverview.qml
      res/qml/Mixxx/Controls/WaveformDisplay.qml
  )
  target_link_libraries(mixxx-qml-lib PRIVATE mixxx-qml-mixxxcontrolsplugin)

  target_sources(
    mixxx-qml-lib
    PRIVATE
      src/qml/asyncimageprovider.cpp
      src/qml/qmlapplication.cpp
      src/qml/qmlautoreload.cpp
      src/qml/qmlbeatsmodel.cpp
      src/qml/qmlchainpresetmodel.cpp
      src/qml/qmlconfigproxy.cpp
      src/qml/qmlcontrolproxy.cpp
      src/qml/qmlcuesmodel.cpp
      src/qml/qmldlgpreferencesproxy.cpp
      src/qml/qmleffectmanifestparametersmodel.cpp
      src/qml/qmleffectslotproxy.cpp
      src/qml/qmleffectsmanagerproxy.cpp
      src/qml/qmllibraryproxy.cpp
      src/qml/qmllibrarysource.cpp
      src/qml/qmllibrarysourcetree.cpp
      src/qml/qmllibrarytracklistmodel.cpp
      src/qml/qmlmixxxcontrollerscreen.cpp
      src/qml/qmlplayermanagerproxy.cpp
      src/qml/qmlplayerproxy.cpp
      src/qml/qmlsidebarmodelproxy.cpp
      src/qml/qmllibrarytracklistcolumn.cpp
      src/qml/qmltrackproxy.cpp
      src/qml/qmlvisibleeffectsmodel.cpp
      src/qml/qmlwaveformdisplay.cpp
      src/qml/qmlwaveformoverview.cpp
      src/qml/qmlwaveformrenderer.cpp
      src/qml/qmlsettingparameter.cpp
      src/qml/qmltrackproxy.cpp
      src/qml/qmlsoundmanagerproxy.cpp
      src/qml/qmlpreferencesproxy.cpp
      src/waveform/renderers/allshader/digitsrenderer.cpp
      src/waveform/renderers/allshader/waveformrenderbeat.cpp
      src/waveform/renderers/allshader/waveformrenderer.cpp
      src/waveform/renderers/allshader/waveformrendererendoftrack.cpp
      src/waveform/renderers/allshader/waveformrendererpreroll.cpp
      src/waveform/renderers/allshader/waveformrendererrgb.cpp
      src/waveform/renderers/allshader/waveformrenderersignalbase.cpp
      src/waveform/renderers/allshader/waveformrendermark.cpp
      src/waveform/renderers/allshader/waveformrendermarkrange.cpp
      src/waveform/renderers/allshader/waveformrendererslipmode.cpp
      src/waveform/renderers/allshader/waveformrendererfiltered.cpp
      src/waveform/renderers/allshader/waveformrendererhsv.cpp
      src/waveform/renderers/allshader/waveformrenderersimple.cpp
      # The following sources need to be in this target to get QML_ELEMENT properly interpreted
      src/control/controlmodel.cpp
      src/control/controlsortfiltermodel.cpp
      # needed for qml/qmlautoreload.cpp
      src/util/autofilereloader.cpp
  )

  if(STEM)
    target_compile_definitions(mixxx-qml-lib PUBLIC __STEM__)
    target_sources(
      mixxx-qml-lib
      PRIVATE
        src/waveform/renderers/allshader/waveformrendererstem.cpp
        src/qml/qmlstemsmodel.cpp
    )
  endif()

  # qt_finalize_target takes care that the resources :/mixxx.org/imports/Mixxx/
  # and :/mixxx.org/imports/Mixxx/Controls are placed into beginning of the binary
  qt_finalize_target(mixxx)

  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/res/qml"
    DESTINATION "${MIXXX_INSTALL_DATADIR}"
  )
endif()

option(DEBUG_ASSERTIONS_FATAL "Fail if debug become true assertions" OFF)
if(DEBUG_ASSERTIONS_FATAL)
  target_compile_definitions(
    mixxx-lib
    PUBLIC MIXXX_DEBUG_ASSERTIONS_FATAL MIXXX_DEBUG_ASSERTIONS_ENABLED
  )
  if(QML)
    target_compile_definitions(
      mixxx-qml-lib
      PUBLIC MIXXX_DEBUG_ASSERTIONS_FATAL MIXXX_DEBUG_ASSERTIONS_ENABLED
    )
  endif()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(
      STATUS
      "DEBUG_ASSERT statements have been enabled because DEBUG_ASSERTIONS_FATAL is ON."
    )
  endif()
endif()

if(EMSCRIPTEN)
  option(
    WASM_ASSERTIONS
    "Enable additional checks when targeting Emscripten/WebAssembly"
    OFF
  )
  if(WASM_ASSERTIONS)
    target_link_options(mixxx-lib PUBLIC -sASSERTIONS)
  endif()
endif()

target_compile_definitions(
  mixxx-lib
  PUBLIC QT_TABLET_SUPPORT QT_USE_QSTRINGBUILDER
)
is_static_library(Qt_IS_STATIC Qt${QT_VERSION_MAJOR}::Core)
if(Qt_IS_STATIC)
  # NOTE(rryan): If you are adding a plugin here, you must also
  # update src/mixxxapplication.cpp to define a Q_IMPORT_PLUGIN
  # for it. Not all imageformats plugins are built as .libs when
  # building Qt statically on Windows. Check the build environment
  # to see exactly what's available as a standalone .lib vs linked
  # into Qt .libs by default.

  target_link_libraries(
    mixxx-lib
    PRIVATE
      # imageformats plugins
      Qt${QT_VERSION_MAJOR}::QGifPlugin
      Qt${QT_VERSION_MAJOR}::QICOPlugin
      Qt${QT_VERSION_MAJOR}::QJpegPlugin
      Qt${QT_VERSION_MAJOR}::QSvgPlugin
      # sqldrivers
      Qt${QT_VERSION_MAJOR}::QSQLiteDriverPlugin
  )

  if(NOT IOS)
    target_link_libraries(
      mixxx-lib
      PRIVATE
        # network plugins
        Qt${QT_VERSION_MAJOR}::QTlsBackendOpenSSLPlugin
    )
  endif()

  if(EMSCRIPTEN)
    target_link_libraries(
      mixxx-lib
      PRIVATE Qt${QT_VERSION_MAJOR}::QWasmIntegrationPlugin
    )
  else()
    target_link_libraries(
      mixxx-lib
      PRIVATE
        # platform plugins
        Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin
        Qt${QT_VERSION_MAJOR}::QMinimalIntegrationPlugin
    )
  endif()

  if(WIN32)
    target_link_libraries(
      mixxx-lib
      PRIVATE Qt${QT_VERSION_MAJOR}::QWindowsIntegrationPlugin
    )
    if(QT_VERSION VERSION_LESS 6.7)
      target_link_libraries(
        mixxx-lib
        PRIVATE Qt${QT_VERSION_MAJOR}::QWindowsVistaStylePlugin
      )
    else()
      target_link_libraries(
        mixxx-lib
        PRIVATE Qt${QT_VERSION_MAJOR}::QModernWindowsStylePlugin
      )
    endif()
  endif()

  if(APPLE)
    if(IOS)
      target_link_libraries(
        mixxx-lib
        PRIVATE Qt${QT_VERSION_MAJOR}::QIOSIntegrationPlugin
      )
    else()
      target_link_libraries(
        mixxx-lib
        PRIVATE
          Qt${QT_VERSION_MAJOR}::QCocoaIntegrationPlugin
          Qt${QT_VERSION_MAJOR}::QMacStylePlugin
      )
    endif()
  endif()
else()
  if(QT6 AND (WIN32 OR APPLE))
    # Qt6 does not automatically install plugins like in Qt 5

    find_package(libjpeg-turbo CONFIG REQUIRED)
    install(
      IMPORTED_RUNTIME_ARTIFACTS
        # QJpegPlugin dependency
        libjpeg-turbo::jpeg
        DESTINATION
        "${MIXXX_INSTALL_DATADIR}"
        COMPONENT
        applocal
    )

    install(
      IMPORTED_RUNTIME_ARTIFACTS
        # platform plugins
        Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin
        Qt${QT_VERSION_MAJOR}::QMinimalIntegrationPlugin
        DESTINATION
        "${MIXXX_INSTALL_DATADIR}/platforms"
        COMPONENT
        applocal
    )

    install(
      IMPORTED_RUNTIME_ARTIFACTS
        Qt${QT_VERSION_MAJOR}::QGifPlugin
        Qt${QT_VERSION_MAJOR}::QICOPlugin
        Qt${QT_VERSION_MAJOR}::QJpegPlugin
        Qt${QT_VERSION_MAJOR}::QSvgPlugin
        DESTINATION
        "${MIXXX_INSTALL_DATADIR}/imageformats"
        COMPONENT
        applocal
    )

    install(
      IMPORTED_RUNTIME_ARTIFACTS
        Qt${QT_VERSION_MAJOR}::QSQLiteDriverPlugin
        DESTINATION
        "${MIXXX_INSTALL_DATADIR}/sqldrivers"
        COMPONENT
        applocal
    )

    if(NOT IOS)
      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QTlsBackendOpenSSLPlugin
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}/tls"
          COMPONENT
          applocal
      )
    endif()

    if(QML)
      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::LabsQmlModels
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}"
          COMPONENT
          applocal
      )

      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QuickControls2Impl
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}"
          COMPONENT
          applocal
      )

      if(QT_VERSION VERSION_GREATER_EQUAL 6.7)
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QuickControls2Basic
            Qt${QT_VERSION_MAJOR}::QuickControls2BasicStyleImpl
            Qt${QT_VERSION_MAJOR}::QuickControls2Fusion
            Qt${QT_VERSION_MAJOR}::QuickControls2FusionStyleImpl
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}"
            COMPONENT
            applocal
        )
      else()
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QuickControls2
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}"
            COMPONENT
            applocal
        )
      endif()

      if(QT_VERSION VERSION_LESS 6.8)
        # From Qt 6.8 integrated in Qml via QmlMeta
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QmlWorkerScript
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}"
            COMPONENT
            applocal
        )
      endif()

      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QuickLayouts
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}"
          COMPONENT
          applocal
      )

      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QuickShapesPrivate
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}"
          COMPONENT
          applocal
      )

      if(QT_VERSION VERSION_LESS 6.4)
        # From Qt 6.4 integrated in QuickControls2
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QuickTemplates2
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}"
            COMPONENT
            applocal
        )
      endif()

      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::ShaderTools
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}"
          COMPONENT
          applocal
      )

      #install qml6-module-qt5compat-graphicaleffects
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/Qt5Compat/GraphicalEffects"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/Qt5Compat"
        COMPONENT applocal
      )

      # install qml6-module-qtmultimedia
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtMultimedia"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml"
        COMPONENT applocal
      )

      # install qml6-module-qtqml-workerscript
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQml/WorkerScript"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQml"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-controls
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Controls"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-layouts
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Layouts"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-nativestyle
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/NativeStyle"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-shapes
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Shapes"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-templates
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Templates"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # qml6-module-qtquick-window
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Window"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qt-labs-qmlmodels
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/Qt/labs/qmlmodels"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/Qt/labs"
        COMPONENT applocal
      )

      # install qml6-module-qtquick-dialogs
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/QtQuick/Dialogs"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/QtQuick"
        COMPONENT applocal
      )

      # install qml6-module-qt-labs-folderlistmodel
      install(
        DIRECTORY
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}$<$<CONFIG:Debug>:/debug>/Qt6/qml/Qt/labs/folderlistmodel"
        DESTINATION "${MIXXX_INSTALL_DATADIR}/Qt6/qml/Qt/labs"
        COMPONENT applocal
      )
    endif()

    if(WIN32)
      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QWindowsIntegrationPlugin
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}/platforms"
          COMPONENT
          applocal
      )
      if(QT_VERSION VERSION_LESS 6.7)
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QWindowsVistaStylePlugin
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}/styles"
            COMPONENT
            applocal
        )
      else()
        install(
          IMPORTED_RUNTIME_ARTIFACTS
            Qt${QT_VERSION_MAJOR}::QModernWindowsStylePlugin
            DESTINATION
            "${MIXXX_INSTALL_DATADIR}/styles"
            COMPONENT
            applocal
        )
      endif()
    endif()
    if(APPLE)
      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QCocoaIntegrationPlugin
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}/platforms"
          COMPONENT
          applocal
      )
      install(
        IMPORTED_RUNTIME_ARTIFACTS
          Qt${QT_VERSION_MAJOR}::QMacStylePlugin
          DESTINATION
          "${MIXXX_INSTALL_DATADIR}/styles"
          COMPONENT
          applocal
      )
    endif()

    set(APPLOCAL_COMPONENT_DEFINED true)
  endif()
endif()

if(APPLE)
  if(Qt_IS_STATIC OR QT6)
    target_link_libraries(
      mixxx-lib
      PRIVATE
        "-weak_framework Accelerate"
        "-weak_framework AudioToolbox"
        "-weak_framework AVFoundation"
        "-weak_framework CoreAudio"
        "-weak_framework CoreFoundation"
        "-weak_framework CoreImage"
        "-weak_framework CoreMedia"
        "-weak_framework CoreMidi"
        "-weak_framework CoreServices"
        "-weak_framework CoreVideo"
        "-weak_framework IOSurface"
        "-weak_framework VideoToolbox"
    )
    if(IOS)
      target_link_libraries(mixxx-lib PRIVATE "-weak_framework UIKit")
    else()
      target_link_libraries(
        mixxx-lib
        PRIVATE "-weak_framework AppKit" "-weak_framework AudioUnit"
      )
    endif()
  else()
    # Used for battery measurements and controlling the screensaver on macOS.
    target_link_libraries(mixxx-lib PRIVATE "-weak_framework IOKit")
  endif()
elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID)
  if(QT6)
    find_package(X11)
  else()
    find_package(X11 REQUIRED)
    find_package(Qt5 COMPONENTS X11Extras REQUIRED)
    target_link_libraries(mixxx-lib PUBLIC Qt5::X11Extras)
  endif()
  if(${X11_FOUND})
    target_include_directories(mixxx-lib SYSTEM PUBLIC "${X11_INCLUDE_DIR}")
    target_link_libraries(mixxx-lib PRIVATE "${X11_LIBRARIES}")
    target_compile_definitions(mixxx-lib PUBLIC __X11__)
  endif()
  find_package(Qt${QT_VERSION_MAJOR} COMPONENTS DBus REQUIRED)
  target_link_libraries(mixxx-lib PUBLIC Qt${QT_VERSION_MAJOR}::DBus)
elseif(WIN32)
  if(Qt_IS_STATIC)
    target_link_libraries(
      mixxx-lib
      PRIVATE
        # Pulled from qt-4.8.2-source\mkspecs\win32-msvc2010\qmake.conf
        # QtCore
        kernel32
        user32 # QtGui, QtOpenGL, libHSS1394
        shell32
        uuid
        ole32 # QtGui,
        advapi32 # QtGui, portaudio, portmidi
        ws2_32 # QtGui, QtNetwork, libshout
        # QtGui
        gdi32 # QtOpenGL, libshout
        comdlg32
        oleaut32
        imm32
        winmm
        winspool
        # QtOpenGL
        glu32
        opengl32
        # QtNetwork openssl-linked
        crypt32
        dwmapi # qtwindows
        iphlpapi # qt5network
        mpr # qt5core
        netapi32 # qt5core
        userenv # qt5core
        uxtheme # ?
        version # ?
        wtsapi32 # ?
    )

    find_library(
      QTFONTDATABASESUPPORT_LIBRARY
      Qt${QT_VERSION_MAJOR}FontDatabaseSupport
    )
    target_link_libraries(mixxx-lib PRIVATE "${QTFONTDATABASESUPPORT_LIBRARY}")
    find_library(
      QTWINDOWSUIAUTOMATIONSUPPORT_LIBRARY
      Qt${QT_VERSION_MAJOR}WindowsUIAutomationSupport
    )
    target_link_libraries(
      mixxx-lib
      PRIVATE "${QTWINDOWSUIAUTOMATIONSUPPORT_LIBRARY}"
    )
    find_library(
      QTEVENTDISPATCHERSUPPORT_LIBRARY
      Qt${QT_VERSION_MAJOR}EventDispatcherSupport
    )
    target_link_libraries(
      mixxx-lib
      PRIVATE "${QTEVENTDISPATCHERSUPPORT_LIBRARY}"
    )
    find_library(QTTHEMESUPPORT_LIBRARY Qt${QT_VERSION_MAJOR}ThemeSupport)
    target_link_libraries(mixxx-lib PRIVATE "${QTTHEMESUPPORT_LIBRARY}")

    find_library(QTFREETYPE_LIBRARY qtfreetype)
    target_link_libraries(mixxx-lib PRIVATE "${QTFREETYPE_LIBRARY}")
    find_library(QTHARFBUZZ_LIBRARY qtharfbuzz)
    target_link_libraries(mixxx-lib PRIVATE "${QTHARFBUZZ_LIBRARY}")
    find_library(QTLIBPNG_LIBRARY qtlibpng)
    target_link_libraries(mixxx-lib PRIVATE "${QTLIBPNG_LIBRARY}")
    find_library(QTPCRE2_LIBRARY qtpcre2)
    target_link_libraries(mixxx-lib PRIVATE "${QTPCRE2_LIBRARY}")
  else()
    #libshout is always built statically
    target_link_libraries(
      mixxx-lib
      PRIVATE
        ws2_32 # libshout
        gdi32 # libshout
    )
  endif()
endif()

if(APPLE OR WIN32)
  # qt_de.qm is just one arbitrary file in the directory that needs to be located;
  # there is no particular reason to look for this file versus any other one in the directory.
  if(QT6)
    find_file(
      QT_TRANSLATION_FILE
      qt_de.qm
      PATHS "${Qt6_DIR}/../../translations/Qt6"
      REQUIRED
      NO_DEFAULT_PATH
    )
  else()
    find_file(
      QT_TRANSLATION_FILE
      qt_de.qm
      PATHS
        "${Qt5_DIR}/../../../translations"
        "${Qt5_DIR}/../../qt5/translations"
      REQUIRED
      NO_DEFAULT_PATH
    )
  endif()
  get_filename_component(QT_TRANSLATIONS ${QT_TRANSLATION_FILE} DIRECTORY)
  install(
    DIRECTORY "${QT_TRANSLATIONS}/"
    DESTINATION "${MIXXX_INSTALL_DATADIR}/translations"
    # QT 5 translations have been separated into several files, and most of the qt_xx.qm files
    # contain just shortcuts to load the qtbase, qtmultimedia etc files.
    FILES_MATCHING
    REGEX
    "qt_.+\.qm|qtbase_.*\.qm|qtmultimedia_.*\.qm|qtscript_.*\.qm|qtxmlpatterns_.*\.qm"
  )
endif()

add_library(
  mixxx-gitinfostore
  STATIC
  EXCLUDE_FROM_ALL
  src/util/gitinfostore.cpp
)
# QtCore for QString
target_link_libraries(mixxx-gitinfostore PUBLIC Qt${QT_VERSION_MAJOR}::Core)
target_include_directories(
  mixxx-gitinfostore
  PUBLIC src ${CMAKE_BINARY_DIR}/src
)
add_dependencies(mixxx-gitinfostore mixxx-gitinfo)

# Queen Mary DSP
add_library(
  QueenMaryDsp
  STATIC
  EXCLUDE_FROM_ALL
  # lib/qm-dsp/base/KaiserWindow.cpp
  lib/qm-dsp/base/Pitch.cpp
  # lib/qm-dsp/base/SincWindow.cpp
  lib/qm-dsp/dsp/chromagram/Chromagram.cpp
  lib/qm-dsp/dsp/chromagram/ConstantQ.cpp
  lib/qm-dsp/dsp/keydetection/GetKeyMode.cpp
  # lib/qm-dsp/dsp/mfcc/MFCC.cpp
  lib/qm-dsp/dsp/onsets/DetectionFunction.cpp
  lib/qm-dsp/dsp/onsets/PeakPicking.cpp
  lib/qm-dsp/dsp/phasevocoder/PhaseVocoder.cpp
  lib/qm-dsp/dsp/rateconversion/Decimator.cpp
  # lib/qm-dsp/dsp/rateconversion/DecimatorB.cpp
  # lib/qm-dsp/dsp/rateconversion/Resampler.cpp
  # lib/qm-dsp/dsp/rhythm/BeatSpectrum.cpp
  # lib/qm-dsp/dsp/segmentation/ClusterMeltSegmenter.cpp
  # lib/qm-dsp/dsp/segmentation/Segmenter.cpp
  # lib/qm-dsp/dsp/segmentation/cluster_melt.c
  # lib/qm-dsp/dsp/segmentation/cluster_segmenter.c
  lib/qm-dsp/dsp/signalconditioning/DFProcess.cpp
  lib/qm-dsp/dsp/signalconditioning/FiltFilt.cpp
  lib/qm-dsp/dsp/signalconditioning/Filter.cpp
  lib/qm-dsp/dsp/signalconditioning/Framer.cpp
  lib/qm-dsp/dsp/tempotracking/DownBeat.cpp
  lib/qm-dsp/dsp/tempotracking/TempoTrack.cpp
  lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp
  lib/qm-dsp/dsp/tonal/ChangeDetectionFunction.cpp
  lib/qm-dsp/dsp/tonal/TCSgram.cpp
  lib/qm-dsp/dsp/tonal/TonalEstimator.cpp
  lib/qm-dsp/dsp/transforms/FFT.cpp
  # lib/qm-dsp/dsp/wavelet/Wavelet.cpp
  lib/qm-dsp/ext/kissfft/kiss_fft.c
  lib/qm-dsp/ext/kissfft/tools/kiss_fftr.c
  # lib/qm-dsp/hmm/hmm.c
  lib/qm-dsp/maths/Correlation.cpp
  # lib/qm-dsp/maths/CosineDistance.cpp
  lib/qm-dsp/maths/KLDivergence.cpp
  lib/qm-dsp/maths/MathUtilities.cpp
  # lib/qm-dsp/maths/pca/pca.c
  # lib/qm-dsp/thread/Thread.cpp
)

target_compile_definitions(QueenMaryDsp PRIVATE kiss_fft_scalar=double)
if(UNIX)
  target_compile_definitions(QueenMaryDsp PRIVATE USE_PTHREADS)
elseif(MSVC)
  # Causes the cmath headers to declare M_PI and friends.
  # http://msdn.microsoft.com/en-us/library/4hwaceh6.aspx We could define this
  # in our headers but then include order matters since headers we don't control
  # may include cmath first.
  target_compile_definitions(QueenMaryDsp PRIVATE _USE_MATH_DEFINES)
endif()
target_include_directories(
  QueenMaryDsp
  SYSTEM
  PUBLIC lib/qm-dsp lib/qm-dsp/include
)
target_link_libraries(mixxx-lib PRIVATE QueenMaryDsp)

# ReplayGain
add_library(ReplayGain STATIC EXCLUDE_FROM_ALL lib/replaygain/replaygain.cpp)
target_include_directories(mixxx-lib SYSTEM PRIVATE lib/replaygain)
target_link_libraries(mixxx-lib PRIVATE ReplayGain)

# Reverb
add_library(Reverb STATIC EXCLUDE_FROM_ALL lib/reverb/Reverb.cc)
if(MSVC)
  target_compile_definitions(Reverb PRIVATE _USE_MATH_DEFINES)
endif()
target_include_directories(Reverb PRIVATE src)
target_link_libraries(Reverb PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_include_directories(mixxx-lib SYSTEM PRIVATE lib/reverb)
target_link_libraries(mixxx-lib PRIVATE Reverb)

# Rubberband
option(RUBBERBAND "Enable the rubberband engine for pitch-bending" ON)
if(RUBBERBAND)
  find_package(rubberband REQUIRED)
  target_link_libraries(mixxx-lib PUBLIC rubberband::rubberband)
  target_compile_definitions(mixxx-lib PUBLIC __RUBBERBAND__)
  if(QML)
    target_link_libraries(mixxx-qml-lib PRIVATE rubberband::rubberband)
    target_compile_definitions(mixxx-qml-lib PUBLIC __RUBBERBAND__)
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/effects/backends/builtin/pitchshifteffect.cpp
      src/engine/bufferscalers/enginebufferscalerubberband.cpp
      src/engine/bufferscalers/rubberbandwrapper.cpp
      src/engine/bufferscalers/rubberbandtask.cpp
      src/engine/bufferscalers/rubberbandworkerpool.cpp
  )
endif()

# SndFile
find_package(SndFile REQUIRED)
target_link_libraries(mixxx-lib PRIVATE SndFile::sndfile)
target_compile_definitions(mixxx-lib PUBLIC __SNDFILE__)
if(SndFile_SUPPORTS_SET_COMPRESSION_LEVEL)
  target_compile_definitions(
    mixxx-lib
    PUBLIC SFC_SUPPORTS_SET_COMPRESSION_LEVEL
  )
endif()

# SoundTouch
find_package(SoundTouch 2.1.2 REQUIRED)
target_link_libraries(mixxx-lib PRIVATE SoundTouch::SoundTouch)

# TagLib
find_package(TagLib 1.11 REQUIRED)
target_link_libraries(mixxx-lib PUBLIC TagLib::TagLib)
if(QML)
  target_link_libraries(mixxx-qml-lib PUBLIC TagLib::TagLib)
endif()

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(mixxx-lib PRIVATE Threads::Threads)

#
# Features
#

# Battery meter
#
# The battery meter is only available on Linux, macOS and Windows, therefore
# this option is forcibly set to OFF on all other platforms.
cmake_dependent_option(
  BATTERY
  "Battery meter support"
  ON
  "WIN32 OR (UNIX AND NOT ANDROID)"
  OFF
)
if(BATTERY)
  if(WIN32)
    target_sources(mixxx-lib PRIVATE src/util/battery/batterywindows.cpp)
  elseif(APPLE)
    if(IOS)
      message(FATAL_ERROR "Battery support is not implemented for iOS")
    else()
      target_sources(mixxx-lib PRIVATE src/util/battery/batterymac.cpp)
    endif()
  elseif(UNIX)
    if(EMSCRIPTEN)
      message(
        FATAL_ERROR
        "Battery support is not implemented for Emscripten (WebAssembly)"
      )
    endif()
    find_package(Upower REQUIRED)
    find_package(GLIB COMPONENTS gobject REQUIRED)
    target_include_directories(mixxx-lib SYSTEM PUBLIC ${GLIB_INCLUDE_DIRS})
    target_link_libraries(
      mixxx-lib
      PRIVATE Upower::Upower ${GLIB_LIBRARIES} ${GLIB_GOBJECT_LIBRARIES}
    )
    target_sources(mixxx-lib PRIVATE src/util/battery/batterylinux.cpp)
  else()
    message(
      FATAL_ERROR
      "Battery support is not implemented for the target platform."
    )
  endif()
  target_compile_definitions(mixxx-lib PUBLIC __BATTERY__)
endif()

# Build Time
option(BUILDTIME "Use __DATE__ and __TIME__" ON)
if(NOT BUILDTIME)
  # Distributions like openSUSE use tools (e. g. build-compare) to detect
  # whether a built binary differs from a former build to avoid unneeded
  # publishing of packages.
  # If __DATE__ and __TIME__ are used the built binary differs always but
  # the tools cannot detect the root and publish a new package although
  # the only change is caused by __DATE__ and __TIME__.
  target_compile_definitions(mixxx-lib PUBLIC DISABLE_BUILDTIME)
endif()

# Clang Color Diagnostics
option(CLANG_COLORDIAG "Clang color diagnostics" OFF)
if(CLANG_COLORDIAG)
  if(NOT LLVM_CLANG)
    message(
      FATAL_ERROR
      "Color Diagnostics are only available when using Clang."
    )
  endif()
  target_compile_options(mixxx-lib PUBLIC -fcolor-diagnostics)
endif()

# Clang Sanitizers
set(SANITIZERS "")
option(SANITIZE_ADDRESS "Address Sanitizer" OFF)
if(SANITIZE_ADDRESS)
  list(APPEND SANITIZERS "address")
endif()
option(SANITIZE_UNDEFINED "Clang Undefined Behaviour Sanitizer" OFF)
if(SANITIZE_UNDEFINED)
  list(APPEND SANITIZERS "undefined")
endif()
option(SANITIZE_THREAD "Clang Thread Sanitizer" OFF)
if(SANITIZE_THREAD)
  list(APPEND SANITIZERS "thread")
endif()
if(NOT SANITIZERS STREQUAL "")
  if(NOT (LLVM_CLANG OR GNU_GCC))
    message(FATAL_ERROR "Sanitizers are only available on Clang or GCC")
  endif()
  list(JOIN SANITIZERS "," SANITZERS_JOINED)
  target_compile_options(mixxx-lib PUBLIC -fsanitize=${SANITZERS_JOINED})
  target_link_options(mixxx-lib PUBLIC -fsanitize=${SANITZERS_JOINED})
endif()

# CoreAudio MP3/AAC Decoder
#
# The CoreAudio API is only available on macOS, therefore this option is
# forcibly set to OFF on all other platforms.
cmake_dependent_option(
  COREAUDIO
  "CoreAudio MP3/AAC Decoder"
  ON
  "APPLE"
  OFF
)
if(COREAUDIO)
  target_sources(
    mixxx-lib
    PRIVATE
      src/sources/soundsourcecoreaudio.cpp
      src/sources/v1/legacyaudiosourceadapter.cpp
      lib/apple/CAStreamBasicDescription.cpp
  )
  set_property(
    SOURCE lib/apple/CAStreamBasicDescription.cpp
    APPEND_STRING
    PROPERTY COMPILE_OPTIONS -Wno-deprecated-anon-enum-enum-conversion
  )
  target_compile_definitions(mixxx-lib PUBLIC __COREAUDIO__)
  target_include_directories(mixxx-lib SYSTEM PUBLIC lib/apple)
endif()

# FAAD AAC audio file decoder plugin
find_package(MP4)
find_package(MP4v2)
# It is enabled by default on Linux only, because other targets have other
# solutions. It requires MP4 or MP4v2.
default_option(FAAD "FAAD AAC audio file decoder support" "UNIX;NOT APPLE;MP4_FOUND OR MP4v2_FOUND")
if(FAAD)
  if(NOT MP4_FOUND AND NOT MP4v2_FOUND)
    message(
      FATAL_ERROR
      "FAAD AAC audio support requires libmp4 or libmp4v2 with development headers."
    )
  endif()
  target_sources(
    mixxx-lib
    PRIVATE src/sources/soundsourcem4a.cpp src/sources/libfaadloader.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __FAAD__)
  if(MP4v2_FOUND)
    target_compile_definitions(mixxx-lib PUBLIC __MP4V2__)
    target_link_libraries(mixxx-lib PRIVATE MP4v2::MP4v2)
  else()
    target_link_libraries(mixxx-lib PRIVATE MP4::MP4)
  endif()
endif()

# FDK-AAC is loaded dynamically at runtime by EncoderFdkAac using QLibrary,
# so copy it into the Windows and macOS packages, but do not link to it.
if(APPLE AND MACOS_BUNDLE)
  find_library(FDK_AAC_LIBRARY fdk-aac)
  if(FDK_AAC_LIBRARY)
    message(STATUS "Found fdk-aac: ${FDK_AAC_LIBRARY}")
    file(
      COPY ${FDK_AAC_LIBRARY}
      DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib/fdk-aac-install"
      FOLLOW_SYMLINK_CHAIN
    )
    install(
      DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/fdk-aac-install/"
      DESTINATION "${MIXXX_INSTALL_PREFIX}/Contents/Frameworks"
    )
  else()
    message(STATUS "Could NOT find libfdk-aac.dylib")
  endif()
elseif(WIN32)
  # On Windows find_library finds the .lib file, but the installer needs the .dll file.
  find_file(FDK_AAC_DLL fdk-aac.dll PATH_SUFFIXES ${CMAKE_INSTALL_BINDIR})
  if(FDK_AAC_DLL)
    message(STATUS "Found fdk-aac DLL: ${FDK_AAC_DLL}")
    install(FILES ${FDK_AAC_DLL} DESTINATION ${MIXXX_INSTALL_BINDIR})
  else()
    message(STATUS "Could NOT find fdk-aac.dll")
  endif()
endif()

# Google PerfTools
option(GPERFTOOLS "Google PerfTools libtcmalloc linkage" OFF)
option(GPERFTOOLSPROFILER "Google PerfTools libprofiler linkage" OFF)
if((BUILD_BENCH) AND (GPERFTOOLS OR GPERFTOOLSPROFILER))
  find_package(GPerfTools REQUIRED)
  if(GPERFTOOLS)
    target_link_libraries(mixxx-lib PRIVATE GPerfTools::tcmalloc)
  endif()
  if(PERFTOOLSPROFILER)
    target_link_libraries(mixxx-lib PRIVATE GPerfTools::profiler)
  endif()
endif()

# HSS1394 MIDI device
#
# The HSS1394 library is only available on macOS, therefore this option is
# forcibly set to OFF on all other platforms.
if(WIN32 OR APPLE)
  find_package(HSS1394)
else()
  set(HSS1394 OFF)
endif()
cmake_dependent_option(
  HSS1394
  "HSS1394 MIDI device support"
  "${HSS1394_FOUND}"
  "WIN32 OR APPLE"
  OFF
)
if(HSS1394)
  target_sources(
    mixxx-lib
    PRIVATE
      src/controllers/midi/hss1394controller.cpp
      src/controllers/midi/hss1394enumerator.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __HSS1394__)
  if(NOT HSS1394_FOUND)
    message(
      FATAL_ERROR
      "HSS1394 MIDI device support requires the libhss1394 and its development headers."
    )
  endif()
  target_link_libraries(mixxx-lib PRIVATE HSS1394::HSS1394)
endif()

# Lilv (LV2)
find_package(lilv)
default_option(LILV "Lilv (LV2) support" "lilv_FOUND")
if(LILV)
  if(NOT lilv_FOUND)
    message(
      FATAL_ERROR
      "Lilv (LV2) support requires the liblilv-0 and LV2 libraries and development headers."
    )
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/effects/backends/lv2/lv2backend.cpp
      src/effects/backends/lv2/lv2effectprocessor.cpp
      src/effects/backends/lv2/lv2manifest.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __LILV__)
  target_link_libraries(mixxx-lib PRIVATE lilv::lilv)
  if(BUILD_TESTING)
    target_link_libraries(mixxx-test PRIVATE lilv::lilv)
  endif()
endif()

# Live Broadcasting (Shoutcast)
cmake_dependent_option(
  BROADCAST
  "Live Broadcasting (Shoutcast) support"
  ON
  "NOT IOS"
  OFF
)
if(BROADCAST)
  find_package(Shoutidjc)
  # Check if system lib is at least 2.4.6 and not suffering bugs
  # https://github.com/mixxxdj/mixxx/issues/9681
  # https://github.com/mixxxdj/mixxx/issues/10305
  if(Shoutidjc_FOUND AND Shoutidjc_VERSION VERSION_LESS 2.4.4)
    message(
      STATUS
      "Installed libshout-idjc version: ${Shoutidjc_VERSION} is suffering from issue #9681"
    )
  elseif(Shoutidjc_FOUND AND Shoutidjc_VERSION VERSION_LESS 2.4.6)
    message(
      STATUS
      "Installed libshout version: ${Shout_VERSION} is suffering from issue #10305"
    )
  endif()
  if(NOT Shoutidjc_FOUND OR Shoutidjc_VERSION VERSION_LESS 2.4.6)
    # Fall back to internal library in the lib tree
    message(STATUS "Using internal libshout-idjc")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib/libshout-idjc")
    target_include_directories(
      mixxx-lib
      SYSTEM
      PUBLIC lib/libshout-idjc/include
    )
    if(WIN32)
      target_compile_definitions(
        shout_mixxx
        PRIVATE __WINDOWS__ _CRT_NONSTDC_NO_WARNINGS
      )
    endif()
    target_link_libraries(mixxx-lib PRIVATE shout_mixxx)
  else()
    target_link_libraries(mixxx-lib PRIVATE Shoutidjc::Shoutidjc)
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/preferences/dialog/dlgprefbroadcastdlg.ui
      src/preferences/dialog/dlgprefbroadcast.cpp
      src/broadcast/broadcastmanager.cpp
      src/engine/sidechain/shoutconnection.cpp
      src/preferences/broadcastprofile.cpp
      src/preferences/broadcastsettings.cpp
      src/preferences/broadcastsettings_legacy.cpp
      src/preferences/broadcastsettingsmodel.cpp
      src/encoder/encoderbroadcastsettings.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __BROADCAST__)
  if(QML)
    target_compile_definitions(mixxx-qml-lib PUBLIC __BROADCAST__)
  endif()
endif()

# Opus (RFC 6716)
find_package(OpusFile)
find_package(Opus)
default_option(OPUS "Opus (RFC 6716) support" "OpusFile_FOUND")
if(OPUS)
  if(NOT OpusFile_FOUND OR NOT Opus_FOUND)
    message(
      FATAL_ERROR
      "Opus support requires libopus and libopusfile with development headers."
    )
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/sources/soundsourceopus.cpp
      src/encoder/encoderopus.cpp
      src/encoder/encoderopussettings.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __OPUS__)
  target_link_libraries(mixxx-lib PRIVATE OpusFile::OpusFile Opus::Opus)
  if(BUILD_TESTING)
    target_link_libraries(mixxx-test PRIVATE OpusFile::OpusFile Opus::Opus)
  endif()
endif()

# MAD MP3 Decoder
find_package(MAD)
find_package(ID3Tag)
default_option(MAD "MAD MP3 Decoder" "MAD_FOUND;ID3Tag_FOUND")
if(MAD)
  if(NOT MAD_FOUND)
    message(
      FATAL_ERROR
      "MAD support requires libmad and its development headers."
    )
  endif()
  if(NOT ID3Tag_FOUND)
    message(
      FATAL_ERROR
      "ID3Tag support requires libid3tag and its development headers."
    )
  endif()
  target_sources(mixxx-lib PRIVATE src/sources/soundsourcemp3.cpp)
  target_compile_definitions(mixxx-lib PUBLIC __MAD__)
  target_link_libraries(mixxx-lib PUBLIC MAD::MAD ID3Tag::ID3Tag)
endif()

# Media Foundation AAC Decoder Plugin
#
# The Media Foundtation API is only available on Windows, therefore this option
# is forcibly set to OFF on all other platforms.
cmake_dependent_option(
  MEDIAFOUNDATION
  "Media Foundation AAC decoder plugin"
  ON
  "WIN32"
  OFF
)
if(MEDIAFOUNDATION)
  find_package(MediaFoundation REQUIRED)
  target_sources(mixxx-lib PRIVATE src/sources/soundsourcemediafoundation.cpp)
  target_compile_definitions(mixxx-lib PUBLIC __MEDIAFOUNDATION__)
  target_include_directories(
    mixxx-lib
    SYSTEM
    PRIVATE ${MediaFoundation_INCLUDE_DIRS}
  )
  target_link_libraries(
    mixxx-lib
    PRIVATE ${MediaFoundation_LIBRARIES} Version.lib
  )
endif()

# Modplug support
find_package(Modplug)
default_option(MODPLUG "Modplug module decoder support" "Modplug_FOUND")
if(MODPLUG)
  if(NOT Modplug_FOUND)
    message(
      FATAL_ERROR
      "Modplug module decoder support requires libmodplug and its development headers."
    )
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/preferences/dialog/dlgprefmodplugdlg.ui
      src/sources/soundsourcemodplug.cpp
      src/preferences/dialog/dlgprefmodplug.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __MODPLUG__)
  target_link_libraries(mixxx-lib PRIVATE Modplug::Modplug)
endif()

find_package(Microsoft.GSL CONFIG)
if(Microsoft.GSL_FOUND)
  target_link_libraries(mixxx-lib PRIVATE Microsoft.GSL::GSL)
  if(QML)
    target_link_libraries(mixxx-qml-lib PRIVATE Microsoft.GSL::GSL)
    target_link_libraries(mixxx-qml-libplugin PRIVATE Microsoft.GSL::GSL)
  endif()
else()
  # check if the headers have been installed without cmake config (< 3.1.0)
  check_include_file_cxx(gsl/gsl HAVE_GSL_GSL)
  if(NOT HAVE_GSL_GSL)
    unset(HAVE_GSL_GSL CACHE) # unset cache to re-evaluate this until it succeeds. check_include_file_cxx() has no REQUIRED flag.
    message(FATAL_ERROR "ms-gsl development headers (libmsgsl-dev) not found")
  endif()
endif()

# QtKeychain
option(
  QTKEYCHAIN
  "Secure credentials storage support for Live Broadcasting profiles"
  ON
)
if(QTKEYCHAIN)
  find_package(Qt${QT_VERSION_MAJOR}Keychain REQUIRED)
  target_compile_definitions(mixxx-lib PUBLIC __QTKEYCHAIN__)
  target_link_libraries(mixxx-lib PRIVATE ${QTKEYCHAIN_LIBRARIES})
  target_include_directories(mixxx-lib SYSTEM PUBLIC ${QTKEYCHAIN_INCLUDE_DIRS})
endif()

# USB HID or/and Bulk controller support
find_package(LibUSB)

# USB HID controller support
option(HID "USB HID controller support" ON)
if(HID)
  # hidapi 0.14.0 is the first release, that contains bus type information
  find_package(hidapi 0.14.0)
  if(NOT hidapi_FOUND)
    message(FATAL_ERROR "hidapi >= 0.14.0 not found!")
  else()
    # hidapi has two backends on Linux, one using the kernel's hidraw API and one using libusb.
    # libusb obviously does not support Bluetooth HID devices, so use the hidraw backend. The
    # libusb backend is the default, so hidraw needs to be selected explicitly at link time.
    if(CMAKE_SYSTEM_NAME STREQUAL Linux)
      target_link_libraries(mixxx-lib PRIVATE hidapi::hidraw)
    else()
      target_link_libraries(mixxx-lib PRIVATE hidapi::hidapi)
    endif()
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/controllers/hid/hidcontroller.cpp
      src/controllers/hid/hidiothread.cpp
      src/controllers/hid/hidioglobaloutputreportfifo.cpp
      src/controllers/hid/hidiooutputreport.cpp
      src/controllers/hid/hiddevice.cpp
      src/controllers/hid/hidenumerator.cpp
      src/controllers/hid/hidreportdescriptor.cpp
      src/controllers/hid/hidusagetables.cpp
      src/controllers/hid/legacyhidcontrollermapping.cpp
      src/controllers/hid/legacyhidcontrollermappingfilehandler.cpp
  )

  if(ANDROID)
    target_sources(mixxx-lib PRIVATE src/controllers/android.cpp)
  else()
    # Android doesn't support QWidget and is not able to compile this manager due to compiler lacking support for structure binding (error: capturing a structured binding is not yet supported)
    target_sources(
      mixxx-lib
      PRIVATE src/controllers/controllerhidreporttabsmanager.cpp
    )
  endif()
  target_compile_definitions(mixxx-lib PUBLIC __HID__)
endif()

# USB Bulk controller support
default_option(BULK "USB Bulk controller support" "LibUSB_FOUND;NOT WIN32")
if(BULK)
  if(NOT LibUSB_FOUND)
    message(
      FATAL_ERROR
      "USB Bulk controller support requires libusb 1.0 and its development headers."
    )
  endif()
  target_sources(
    mixxx-lib
    PRIVATE
      src/controllers/bulk/bulkcontroller.cpp
      src/controllers/bulk/bulkenumerator.cpp
  )
  if(NOT HID)
    target_sources(
      mixxx-lib
      PRIVATE
        src/controllers/hid/legacyhidcontrollermapping.cpp
        src/controllers/hid/legacyhidcontrollermappingfilehandler.cpp
    )
  endif()
  target_compile_definitions(mixxx-lib PUBLIC __BULK__)
  target_link_libraries(mixxx-lib PRIVATE LibUSB::LibUSB)
endif()

# Vinyl Control
default_option(VINYLCONTROL "Vinyl Control support" "NOT MACAPPSTORE")
if(VINYLCONTROL)
  if(MACAPPSTORE)
    message(
      FATAL_ERROR
      "Mac App Store and Vinyl Control support are mutually exclusive due to licensing issues."
    )
  endif()

  target_sources(
    mixxx-lib
    PRIVATE
      src/vinylcontrol/vinylcontrol.cpp
      src/vinylcontrol/vinylcontrolxwax.cpp
      src/preferences/dialog/dlgprefvinyl.cpp
      src/vinylcontrol/vinylcontrolsignalwidget.cpp
      src/vinylcontrol/vinylcontrolmanager.cpp
      src/vinylcontrol/vinylcontrolprocessor.cpp
      src/vinylcontrol/steadypitch.cpp
      src/engine/controls/vinylcontrolcontrol.cpp
  )
  target_compile_definitions(mixxx-lib PUBLIC __VINYLCONTROL__)

  # Internal xwax library
  add_library(mixxx-xwax STATIC EXCLUDE_FROM_ALL)
  target_sources(
    mixxx-xwax
    PRIVATE
      lib/xwax/timecoder.c
      lib/xwax/timecoder_mk2.c
      lib/xwax/lut.c
      lib/xwax/lut_mk2.c
      lib/xwax/filters.c
      lib/xwax/ringbuffer.c
      lib/xwax/pitch_kalman.c
  )
  target_include_directories(mixxx-xwax SYSTEM PUBLIC lib/xwax)
  target_link_libraries(mixxx-lib PRIVATE mixxx-xwax)
endif()

# rendergraph
add_subdirectory(src/rendergraph)
target_compile_definitions(
  rendergraph_gl
  PUBLIC $<$<CONFIG:Debug>:MIXXX_DEBUG_ASSERTIONS_ENABLED>
)
target_link_libraries(mixxx-lib PUBLIC rendergraph_gl)
target_compile_definitions(mixxx-lib PUBLIC rendergraph=rendergraph_gl)
target_compile_definitions(mixxx-lib PRIVATE allshader=allshader_gl)
if(BUILD_TESTING)
  target_compile_definitions(mixxx-test PRIVATE allshader=allshader_gl)
endif()
if(QML)
  target_compile_definitions(
    rendergraph_sg
    PUBLIC $<$<CONFIG:Debug>:MIXXX_DEBUG_ASSERTIONS_ENABLED>
  )
  target_link_libraries(mixxx-qml-lib PRIVATE rendergraph_sg)
  target_compile_definitions(mixxx-qml-lib PRIVATE rendergraph=rendergraph_sg)
  target_compile_definitions(mixxx-qml-lib PRIVATE __SCENEGRAPH__)
  target_compile_definitions(mixxx-qml-lib PRIVATE allshader=allshader_sg)
endif()

# WavPack audio file support
find_package(wavpack)
default_option(WAVPACK "WavPack audio file support" "wavpack_FOUND")
if(WAVPACK)
  if(NOT wavpack_FOUND)
    message(
      FATAL_ERROR
      "WavPack audio file support requires libwv and its development headers."
    )
  endif()
  target_sources(mixxx-lib PRIVATE src/sources/soundsourcewv.cpp)
  target_compile_definitions(mixxx-lib PUBLIC __WV__)
  target_link_libraries(mixxx-lib PRIVATE WavPack::wavpack)
endif()

target_precompile_headers(
  mixxx-lib
  PUBLIC ${MIXXX_LIB_PRECOMPILED_HEADER} ${MIXXX_COMMON_PRECOMPILED_HEADER}
)
if(BUILD_TESTING)
  target_precompile_headers(mixxx-test REUSE_FROM mixxx-lib)
endif()

# Configure file with build options
file(
  RELATIVE_PATH
  MIXXX_INSTALL_DOCDIR_RELATIVE_TO_DATADIR
  "${CMAKE_INSTALL_PREFIX}/${MIXXX_INSTALL_DATADIR}"
  "${CMAKE_INSTALL_PREFIX}/${MIXXX_INSTALL_DOCDIR}"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/config.h"
  @ONLY
)

# Packaging
set(CPACK_PACKAGE_NAME "Mixxx")
set(CPACK_PACKAGE_VENDOR "Mixxx Project")
set(CPACK_PACKAGE_CONTACT "RJ Skerry-Ryan <rryan@mixxx.org>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Digital DJ Application")
set(
  CPACK_PACKAGE_DESCRIPTION_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackPackageDescription.txt"
)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Mixxx")
set(CPACK_PACKAGE_EXECUTABLES "mixxx;Mixxx")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/res/images/mixxx_install_logo.bmp")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.mixxx.org/")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_STRIP_FILES ON)
set(CPACK_CREATE_DESKTOP_LINKS "mixxx")
set(CPACK_MIXXX_VERSION "${MIXXX_VERSION}")
# Save GIT values just in case they have been set manual via cmake
set(CPACK_GIT_DESCRIBE "${GIT_DESCRIBE}")
set(CPACK_GIT_COMMIT_DATE ${GIT_COMMIT_DATE})

# Detailed version information, git info and package file name are set from
# CPackConfig.cmake, not here.

set(CPACK_SOURCE_IGNORE_FILES "\\\\.#;/#;.*~;\\\\.o$")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.git/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.github/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/build/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_BINARY_DIR}/")
set(CPACK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
if(QT6)
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-translations-l10n")
else()
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qttranslations5-l10n")
endif()
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "pdf-viewer, pulseaudio-utils")
set(CPACK_DEBIAN_PACKAGE_REPLACES "mixxx-data")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "fonts-open-sans")
list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "fonts-ubuntu")
if(QT6)
  list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite")
  list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
  if(QT_VERSION VERSION_GREATER_EQUAL 6.8)
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-svg-plugins")
  endif()
  if(QML)
    list(
      APPEND
      CPACK_DEBIAN_PACKAGE_DEPENDS
      "qml6-module-qt-labs-folderlistmodel"
    )
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qt-labs-qmlmodels")
    list(
      APPEND
      CPACK_DEBIAN_PACKAGE_DEPENDS
      "qml6-module-qt5compat-graphicaleffects"
    )
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtmultimedia")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtqml-workerscript")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-controls")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-dialogs")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-layouts")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-shapes")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-templates")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick-window")
  endif()
else()
  list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5sql5-sqlite")
  if(QML)
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml-module-qtquick-controls")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml-module-qtquick-controls2")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml-module-qt-labs-qmlmodels")
    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS "qml-module-qtquick-shapes")
  endif()
endif()
list(JOIN CPACK_DEBIAN_PACKAGE_DEPENDS ", " CPACK_DEBIAN_PACKAGE_DEPENDS)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
file(READ ${CPACK_PACKAGE_DESCRIPTION_FILE} CPACK_DEBIAN_PACKAGE_DESCRIPTION)
set(
  CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED
  "${CPACK_DEBIAN_PACKAGE_DESCRIPTION}"
)
string(
  PREPEND
  CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED
  "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}"
  "\n"
)
string(
  REPLACE
  "\n\n"
  "\n.\n"
  CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED
  "${CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED}"
)
string(
  REPLACE
  "\n"
  "\n "
  CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED
  "${CPACK_DEBIAN_PACKAGE_DESCRIPTION_MERGED}"
)

# This is the version of the package itself and can be advanced or set to
# something like 0ubuntu1 when building a new package from the same version
if(NOT CPACK_DEBIAN_PACKAGE_RELEASE)
  set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
endif()

set(CPACK_DEBIAN_DISTRIBUTION_RELEASES noble questing resolute)
set(CPACK_DEBIAN_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(
  CPACK_DEBIAN_UPLOAD_PPA_SCRIPT
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackDebUploadPPA.cmake"
)
set(
  CPACK_DEBIAN_INSTALL_SCRIPT
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackDebInstall.cmake"
)

set(CPACK_WIX_UPGRADE_GUID "921DC99C-4DCF-478D-B950-50685CB9E6BE")
set(
  CPACK_WIX_LICENSE_RTF
  "${CMAKE_CURRENT_BINARY_DIR}/packaging/wix/LICENSE.rtf"
)
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/res/images/icons/ic_mixxx.ico")
set(CPACK_WIX_PROPERTY_ARPHELPLINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(
  CPACK_WIX_UI_BANNER
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/wix/images/banner.bmp"
)
set(
  CPACK_WIX_UI_DIALOG
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/wix/images/dialog.bmp"
)

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/packaging/CPackConfig.cmake")

if(WIN32)
  # override not working default NSIS
  set(CPACK_GENERATOR WIX)

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
    if(${CMAKE_VERSION} VERSION_LESS "3.24")
      message(FATAL_ERROR "WIX: Need CMake version >= 3.24 for ARM64")
    endif()
    set(CPACK_WIX_ARCHITECTURE "ARM64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x64|x86_64|AMD64)$")
    set(CPACK_WIX_ARCHITECTURE "x64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3456]86|x86)$")
    set(CPACK_WIX_ARCHITECTURE "x86")
  else()
    message(
      FATAL_ERROR
      "WIX: Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}"
    )
  endif()

  # uses CMAKE_PROJECT_VERSION
  configure_file(packaging/wix/LICENSE.rtf.in packaging/wix/LICENSE.rtf @ONLY)
endif()

include(CPack)

if(APPLOCAL_COMPONENT_DEFINED)
  cpack_add_component(applocal HIDDEN REQUIRED)

  # In order to run Mixx from the build directory install applocal components
  add_custom_command(
    TARGET mixxx
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -DCOMPONENT=applocal
      -DCMAKE_INSTALL_PREFIX="${CMAKE_CURRENT_BINARY_DIR}" -P
      cmake_install.cmake
    COMMENT
      "Install applocal components to allow Mixxx to be run from the build directory."
  )
endif()

if(APPLE AND MACOS_BUNDLE)
  set(BUNDLE_NAME "${MIXXX_INSTALL_PREFIX}")
  foreach(PREFIX ${CMAKE_PREFIX_PATH})
    list(APPEND BUNDLE_DIRS "${PREFIX}/lib")
  endforeach()
  set(
    APPLE_CODESIGN_ENTITLEMENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Mixxx.entitlements"
  )

  # Starting with arm64 macOS Apple will require ad-hoc code signatures,
  # which can be generated by setting the identity to a single dash (-).
  # These only include a checksum for verifying integrity, not an actual
  # signature.
  if(NOT APPLE_CODESIGN_IDENTITY)
    set(APPLE_CODESIGN_IDENTITY -)
  endif()

  configure_file(
    cmake/modules/BundleInstall.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/BundleInstall.cmake"
    @ONLY
  )
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/BundleInstall.cmake")
endif()

option(NETWORKAUTH "Enable Spotify AND YouTube features" OFF)
if(NETWORKAUTH)
  find_package(Qt${QT_VERSION_MAJOR} COMPONENTS NetworkAuth QUIET)
  if(Qt${QT_VERSION_MAJOR}NetworkAuth_FOUND)
    target_compile_definitions(mixxx-lib PUBLIC NETWORKAUTH)
    list(APPEND QT_EXTRA_COMPONENTS "NetworkAuth")
    target_link_libraries(mixxx-lib PUBLIC Qt${QT_VERSION_MAJOR}::NetworkAuth)
  endif()
endif()
