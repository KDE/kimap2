# FIXME: Make it build with this flag
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

# Turn exceptions on
kde_enable_exceptions()

set(kimap_SRCS
   acl.cpp
   acljobbase.cpp
   appendjob.cpp
   capabilitiesjob.cpp
   closejob.cpp
   copyjob.cpp
   createjob.cpp
   deleteacljob.cpp
   deletejob.cpp
   expungejob.cpp
   fetchjob.cpp
   getacljob.cpp
   getmetadatajob.cpp
   getquotajob.cpp
   getquotarootjob.cpp
   idjob.cpp
   idlejob.cpp
   imapset.cpp
   imapstreamparser.cpp
   job.cpp
   listjob.cpp
   listrightsjob.cpp
   loginjob.cpp
   logoutjob.cpp
   metadatajobbase.cpp
   myrightsjob.cpp
   namespacejob.cpp
   quotajobbase.cpp
   renamejob.cpp
   rfccodecs.cpp
   searchjob.cpp
   selectjob.cpp
   session.cpp
   sessionlogger.cpp
   setacljob.cpp
   setmetadatajob.cpp
   setquotajob.cpp
   statusjob.cpp
   storejob.cpp
   subscribejob.cpp
   unsubscribejob.cpp
)

ecm_qt_declare_logging_category(kimap_SRCS HEADER kimap_debug.h IDENTIFIER KIMAP2_LOG CATEGORY_NAME log_kimap2)

add_library(KIMAP2 ${kimap_SRCS})

generate_export_header(KIMAP2 BASE_NAME kimap2)

target_include_directories(KIMAP2 INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/KIMAP2;${Sasl2_INCLUDE_DIR}>")
target_include_directories(KIMAP2 PUBLIC "$<BUILD_INTERFACE:${KIMAP2_SOURCE_DIR}/src;${KIMAP2_BINARY_DIR}/src>")

target_link_libraries(KIMAP2
PUBLIC
  KF5::Mime
PRIVATE
  Qt5Network
  KF5::Codecs
  KF5::CoreAddons
  ${Sasl2_LIBRARIES}
)

set_target_properties(KIMAP2 PROPERTIES
    VERSION ${KIMAP2_VERSION_STRING}
    SOVERSION ${KIMAP2_SOVERSION}
    EXPORT_NAME KIMAP2
)

ecm_generate_headers(KIMAP2_CamelCase_HEADERS
  HEADER_NAMES
  Acl
  AclJobBase
  AppendJob
  CapabilitiesJob
  CloseJob
  CopyJob
  CreateJob
  DeleteAclJob
  DeleteJob
  ExpungeJob
  FetchJob
  GetAclJob
  GetMetaDataJob
  GetQuotaJob
  GetQuotaRootJob
  IdJob
  IdleJob
  ImapSet
  Job
  ListJob
  ListRightsJob
  LoginJob
  LogoutJob
  MetaDataJobBase
  MyRightsJob
  QuotaJobBase
  RenameJob
  SearchJob
  SelectJob
  Session
  SetAclJob
  SetMetaDataJob
  SetQuotaJob
  StatusJob
  StoreJob
  SubscribeJob
  UnsubscribeJob
  PREFIX KIMAP2
  REQUIRED_HEADERS KIMAP2_HEADERS
)

install(TARGETS KIMAP2 EXPORT KIMAP2Targets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kimap2_export.h
    ${KIMAP2_HEADERS}
    namespacejob.h rfccodecs.h # FIXME these two headers don't have camelcase headers in KDE4
    ${KIMAP2_CamelCase_HEADERS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kimap2
    COMPONENT Devel
)

install(FILES
    ${KIMAP2_CamelCase_HEADERS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/KIMAP2
    COMPONENT Devel
)


ecm_generate_pri_file(BASE_NAME KIMAP2 LIB_NAME KIMAP2 DEPS "Mime" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR}/KIMAP2)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
