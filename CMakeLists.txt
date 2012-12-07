# CMake build script for Chok!
# x64 Windows Server-only
# 2012/02/26 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8)

project (Chok)

# Net-SNMP SDK installed, tested 5.7.1.
if (MSVC11)
	set(NETSNMP_BUILD_COMPILER "vc110")
elseif (MSVC10)
	set(NETSNMP_BUILD_COMPILER "vc100")
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NETSNMP_BUILD_TYPE "debug")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NETSNMP_BUILD_TYPE "release")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(NETSNMP_ROOT D:/net-snmp-5.7.1/stage/${NETSNMP_BUILD_COMPILER}/x64/${NETSNMP_BUILD_TYPE})
set(NETSNMP_INCLUDE_DIRS ${NETSNMP_ROOT}/include)
set(NETSNMP_LIBRARY_DIRS ${NETSNMP_ROOT}/lib)
set(NETSNMP_LIBRARIES
	netsnmp.lib
	netsnmpagent.lib
	netsnmpmibs.lib
)

# Thomson Reuters Robust Foundation API
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(RFA_BUILD_TYPE "Debug_MDd")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(RFA_BUILD_TYPE "Release_MD")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(RFA_ROOT D:/rfa7.4.0.L1.win-shared.rrg)
set(RFA_INCLUDE_DIRS
	${RFA_ROOT}/Include
	${RFA_ROOT}/Include/rwf
)
set(RFA_LIBRARY_DIRS ${RFA_ROOT}/Libs/WIN_64_VS100/${RFA_BUILD_TYPE})
set(RFA_LIBRARY_DIR ${RFA_LIBRARY_DIRS})
set(RFA_LIBRARIES
        RFA7_Common100_x64.lib
        RFA7_Config100_x64.lib
        RFA7_Logger100_x64.lib
        RFA7_Data100_x64.lib
        RFA7_Connections100_x64.lib
        RFA7_Connections_OMM100_x64.lib
        RFA7_SessionLayer100_x64.lib
)
set(RFA_RUNTIME_LIBRARIES
	${RFA_LIBRARY_DIR}/RFA7_Common100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_Config100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_Logger100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_Data100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_Connections100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_Connections_OMM100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_SessionLayer_OMM100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_SessionLayer100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_RSSL_Adapter100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_RSSL_Cons_Adapter100_x64.dll
	${RFA_LIBRARY_DIR}/RFA7_RSSL_Prov_Adapter100_x64.dll
)
set(BOOST_ROOT D:/boost_1_51_0)
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib)
set(Boost_USE_STATIC_LIBS ON)
find_package (Boost 1.44 COMPONENTS thread REQUIRED)

#-----------------------------------------------------------------------------
# force off-tree build

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "CMake generation is not allowed within the source directory!
Remove the CMakeCache.txt file and try again from another folder, e.g.:

   del CMakeCache.txt
   mkdir build
   cd build
   cmake ..
")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

#-----------------------------------------------------------------------------
# default to Release build

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------
# platform specifics

add_definitions(
	-DWIN32
	-DWIN32_LEAN_AND_MEAN
# Windows Server 2008 R2
	-D_WIN32_WINNT=0x0601
# Net-SNMP Autoconf overrides
	-DHAVE_STDINT_H
	-DHAVE_SOCKLEN_T
# RFA version
        -DRFA_LIBRARY_VERSION="7.4.0."
# production release
#	-DOFFICIAL_BUILD
#	-DENABLE_LEAK_TRACKER
)

# SEH Exceptions.
string(REGEX REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Parallel make.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Optimization flags.
# http://msdn.microsoft.com/en-us/magazine/cc301698.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

# Disable buffer security check.
# http://msdn.microsoft.com/en-us/library/8dbf701c(v=vs.80).aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS-")

#-----------------------------------------------------------------------------
# source files

set(cxx-sources
	src/chok.cc
	src/chokMIB.cc
	src/config.cc
	src/consumer.cc
	src/error.cc
	src/main.cc
	src/rfa.cc
	src/rfa_logging.cc
	src/snmp_agent.cc
	src/chromium/chromium_switches.cc
	src/chromium/command_line.cc
	src/chromium/debug/stack_trace.cc
	src/chromium/debug/stack_trace_win.cc
	src/chromium/memory/singleton.cc
	src/chromium/metrics/histogram.cc
	src/chromium/logging.cc
	src/chromium/logging_win.cc
	src/chromium/string_piece.cc
	src/chromium/string_split.cc
	src/chromium/string_util.cc
	src/chromium/stringprintf.cc
	src/chromium/synchronization/lock.cc
	src/chromium/synchronization/lock_impl_win.cc
	src/chromium/vlog.cc
	src/chromium/win/event_trace_provider.cc
)

set(rc-sources
	src/version.rc
)

include_directories(
	include
	${NETSNMP_INCLUDE_DIRS}
	${RFA_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${NETSNMP_LIBRARY_DIRS}
	${RFA_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
)

#-----------------------------------------------------------------------------
# output

add_executable(Chok ${cxx-sources} ${rc-sources})

target_link_libraries(Chok
	${NETSNMP_LIBRARIES}
	${RFA_LIBRARIES}
	${Boost_LIBRARIES}
	ws2_32.lib
	dbghelp.lib
)

file(GLOB mibs "${CMAKE_CURRENT_SOURCE_DIR}/mibs/*.txt")

install (TARGETS Chok DESTINATION bin)
install (FILES ${RFA_RUNTIME_LIBRARIES} DESTINATION bin)
install (FILES ${mibs} DESTINATION mibs)

set (CPACK_SOURCE_IGNORE_FILES "*.lib")

include (InstallRequiredSystemLibraries)
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chok!")
set (CPACK_PACKAGE_VENDOR "Thomson Reuters")
set (CPACK_NSIS_CONTACT "Steven McCoy <Steven.McCoy@thomsonreuters.com>")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
set (CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\branding.bmp")
set (CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "0")
set (CPACK_PACKAGE_VERSION_PATCH "1")
include (CPack)

# end of file
