include(ExternalProject)

option(DOWNLOAD_OSS_FUZZ "Clone the OSS-Fuzz harness when configuring the project" OFF)

set(OS_FUZZ_SOURCE_DIR "${CMAKE_BINARY_DIR}/oss-fuzz")
set(OS_FUZZ_REPO "https://github.com/ninacatcoin/oss-fuzz.git")
set(OS_FUZZ_TAG "master")

if(DOWNLOAD_OSS_FUZZ)
  ExternalProject_Add(
    oss_fuzz_harness
    PREFIX "${OS_FUZZ_SOURCE_DIR}"
    GIT_REPOSITORY "${OS_FUZZ_REPO}"
    GIT_TAG "${OS_FUZZ_TAG}"
    GIT_SHALLOW TRUE
    SOURCE_DIR "${OS_FUZZ_SOURCE_DIR}"
    BINARY_DIR "${OS_FUZZ_SOURCE_DIR}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
  )

  add_custom_target(download_oss_fuzz ALL DEPENDS oss_fuzz_harness)
else()
  add_custom_target(download_oss_fuzz
    COMMENT "Skipping OSS-Fuzz clone (DOWNLOAD_OSS_FUZZ=OFF)"
  )
endif()
