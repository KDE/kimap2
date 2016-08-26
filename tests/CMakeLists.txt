kde_enable_exceptions()

find_package(Qt5Test CONFIG REQUIRED) #for qsignalspy.h

add_executable(testimapidle testimapidle.cpp)
target_link_libraries(testimapidle KIMAP2 Qt5::Test Qt5::Network KF5::CoreAddons)

# add_executable(testimapserver testimapserver.cpp)
# target_link_libraries(testimapserver KIMAP2 Qt5::Test Qt5::Network KF5::CoreAddons)
