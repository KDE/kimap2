kde_enable_exceptions()

include(ECMMarkAsTest)

find_package(Qt5Test CONFIG REQUIRED)

add_subdirectory(kimap2test)

remove_definitions(-DQT_USE_QSTRINGBUILDER)
macro(KIMAP2_UNIT_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    add_test(${_testname} ${_testname})
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname} KIMAP2 Qt5::Test kimap2test Qt5::Network KF5::CoreAddons)
    set_target_properties(${_testname} PROPERTIES COMPILE_FLAGS -DTEST_DATA="\\"${CMAKE_CURRENT_SOURCE_DIR}\\"")
  endforeach()
endmacro()

########### automated tests ###############

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

KIMAP2_UNIT_TESTS(
  fakeservertest
  testrfccodecs
  testsession
  loginjobtest
  logoutjobtest
  capabilitiesjobtest
  selectjobtest
  createjobtest
  deletejobtest
  fetchjobtest
  renamejobtest
  subscribejobtest
  unsubscribejobtest
  listjobtest
  storejobtest
  imapsettest
  idjobtest
  idlejobtest
  quotarootjobtest
  searchjobtest
  getmetadatajobtest
  streamparsertest
  setmetadatajobtest
  appendjobtest
  statusjobtest
)
