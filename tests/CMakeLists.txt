kde_enable_exceptions()

find_package(Qt5Test CONFIG REQUIRED) #for qsignalspy.h

add_executable(testimapidle testimapidle.cpp)
target_link_libraries(testimapidle KIMAP2 Qt5::Test Qt5::Network KF5::CoreAddons)

add_executable(imapcmd imapcmd.cpp)
target_link_libraries(imapcmd KIMAP2 Qt5::Test Qt5::Network KF5::CoreAddons)

# add_executable(testimapserver testimapserver.cpp)
# target_link_libraries(testimapserver KIMAP2 Qt5::Test Qt5::Network KF5::CoreAddons)

include(ECMMarkAsTest)

remove_definitions(-DQT_USE_QSTRINGBUILDER)
macro(KIMAP2_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    target_link_libraries(${_testname} KIMAP2 Qt5::Test kimap2test Qt5::Network KF5::CoreAddons)
  endforeach()
endmacro()

########### automated tests ###############

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

KIMAP2_TESTS(
    benchmark
)
