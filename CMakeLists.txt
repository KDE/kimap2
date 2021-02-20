cmake_minimum_required(VERSION 2.8.12)

project(KIMAP2)

# ECM setup
find_package(ECM 5.19.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${KIMAP2_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)
include(ECMPackageConfigHelpers)
include(ECMSetupVersion)
include(FeatureSummary)
include(ECMQtDeclareLoggingCategory)
include(ECMCoverageOption)

set(KIMAP2_LIB_VERSION "0.4.0")
ecm_setup_version(${KIMAP2_LIB_VERSION} VARIABLE_PREFIX KIMAP2
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kimap2_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KIMAP2ConfigVersion.cmake"
                        SOVERSION 0
)

########### Find packages ###########
set(KF5CoreAddons_VERSION "5.19.0")
set(KF5Mime_VERSION "5.1.40")

find_package(Qt5 5.2 COMPONENTS REQUIRED Core Network)
find_package(KF5CoreAddons ${KF5CoreAddons_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs "5.19.0" CONFIG REQUIRED)
find_package(KF5Mime ${KF5Mime_VERSION} CONFIG REQUIRED)

find_package(Sasl2)
set_package_properties(Sasl2 PROPERTIES
                             DESCRIPTION "The Cyrus-sasl library"
                             URL "http://www.cyrussasl.org"
                             TYPE REQUIRED
)

########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KIMAP2")

ecm_configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KIMAP2Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KIMAP2Config.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

########### Targets ###########
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

add_subdirectory(cmake)

########### Install Files ###########
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KIMAP2Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KIMAP2ConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KIMAP2Targets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KIMAP2Targets.cmake)

install(FILES
   ${CMAKE_CURRENT_BINARY_DIR}/kimap2_version.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
