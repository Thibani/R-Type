# Third-party dependencies, fetched at configure time with CPM.cmake.
# Nothing third-party is ever copied into the repository.
#
# Set CPM_SOURCE_CACHE (environment or cache variable) to share downloads
# between build directories and avoid re-downloading on every clean build.

set(CPM_DOWNLOAD_VERSION 0.40.8)
set(CPM_DOWNLOAD_HASH 78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791)

# CPM itself always lives in the build directory. Its location must stay the
# same between two configure runs, otherwise CPM refuses to load a second time.
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${CPM_DOWNLOAD_LOCATION}"
        EXPECTED_HASH SHA256=${CPM_DOWNLOAD_HASH})
endif()

include("${CPM_DOWNLOAD_LOCATION}")

# --- Test dependencies --------------------------------------------------------

if(RTYPE_BUILD_TESTS)
    CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.15.2
        OPTIONS
            "INSTALL_GTEST OFF"
            "BUILD_GMOCK OFF"
            "gtest_force_shared_crt ON") # Match the MSVC runtime used by our targets
endif()
