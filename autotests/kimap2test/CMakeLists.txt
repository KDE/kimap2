kde_enable_exceptions()

remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

set(kimap2test_SRCS
   fakeserver.cpp
   mockjob.cpp
   sslserver.cpp
)

add_library(kimap2test STATIC ${kimap2test_SRCS})
generate_export_header(kimap2test)
target_link_libraries(kimap2test KIMAP2 Qt5::Test Qt5::Network Qt5::Core KF5::CoreAddons)

install(TARGETS kimap2test ${INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install(FILES
  fakeserver.h
  mockjob.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kimap2test COMPONENT Devel)
