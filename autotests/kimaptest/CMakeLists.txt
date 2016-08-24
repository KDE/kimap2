kde_enable_exceptions()

remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

set(kimaptest_SRCS
   fakeserver.cpp
   mockjob.cpp
   sslserver.cpp
)

add_library(kimaptest STATIC ${kimaptest_SRCS})
generate_export_header(kimaptest)
target_link_libraries(kimaptest KF5IMAP Qt5::Test Qt5::Network Qt5::Core KF5::CoreAddons)

install(TARGETS kimaptest ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install(FILES
  fakeserver.h
  mockjob.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/kimaptest COMPONENT Devel)
