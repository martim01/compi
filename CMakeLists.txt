cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(compi LANGUAGES CXX C)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#log
message("Does pml::log exist?")
find_path(LOG_LIB_DIR NAMES "include/log.h" PATHS ${PROJECT_SOURCE_DIR}/external/log)
if((NOT LOG_LIB_DIR) OR (NOT EXISTS ${LOG_LIB_DIR}))
    # we couldn't find the header files for FOO or they don't exist
    message("Unable to find pml::log")
    
    # we have a submodule setup for pml_log
    # now we need to clone this submodule
    execute_process(COMMAND git submodule update --init -- external/log WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    find_path(LOG_LIB_DIR NAMES "include/log.h" PATHS ${PROJECT_SOURCE_DIR}/external/log)
else()
    message("Found pml::log")
endif()

#kissfft
message("Does kissfft exist?")
find_path(KISS_LIB_DIR NAMES "kiss_fft.c" PATHS ${PROJECT_SOURCE_DIR}/external/kissfft)
if((NOT KISS_LIB_DIR) OR (NOT EXISTS ${KISS_LIB_DIR}))
    # we couldn't find the header files for FOO or they don't exist
    message("Unable to find kissfft")
    
    # we have a submodule setup for kissfft
    # now we need to clone this submodule
    execute_process(COMMAND git submodule update --init -- external/kissfft WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    find_path(KISS_LIB_DIR NAMES "kiss_fft.c" PATHS ${PROJECT_SOURCE_DIR}/external/kissfft)
else()
    message("Found kissfft")
endif()


#agent++
message("Does Agent++ exist?")
find_path(AGENT_LIB_DIR NAMES "agent++.h" PATHS /usr/local/include/agent_pp)
if((NOT AGENT_LIB_DIR) OR (NOT EXISTS ${AGENT_LIB_DIR}))
	message(FATAL_ERROR "Unable to find Agent++")	
endif()

#snmp++
message("Does SNMP++ exist?")
find_path(AGENT_LIB_DIR NAMES "snmp_pp.h" PATHS /usr/local/include/snmp_pp)
if((NOT AGENT_LIB_DIR) OR (NOT EXISTS ${AGENT_LIB_DIR}))
	message(FATAL_ERROR "Unable to find SNMP++")	
endif()


add_executable(compi "external/kissfft/kiss_fft.c"
                     "external/kissfft/tools/kiss_fftnd.c"
                     "external/kissfft/tools/kiss_fftndr.c"
                     "external/kissfft/tools/kiss_fftr.c"
                     "external/log/src/log.cpp"
                     "external/phash/audiophash.cpp"
                     "external/phash/ph_fft.cpp"
                     "src/agentthread.cpp"
                     "src/compi.cpp"
                     "src/hash.cpp"
                     "src/inimanager.cpp"
                     "src/inisection.cpp"
                     "src/kiss_xcorr.c"
                     "src/logtofile.cpp"
                     "src/main.cpp"
                     "src/recorder.cpp"
                     "src/mibwritabletable.cpp"
                     "src/utils.cpp")


list(APPEND flags "-fPIC" "-Wall" "-fpermissive" "-O3" "-pthread" "-std=c++14")
target_compile_options(compi PRIVATE ${flags})
target_compile_definitions(compi PUBLIC NDEBUG)

message("Does PortAudio exist?")
find_package(PkgConfig REQUIRED QUIET)
pkg_search_module(portaudio REQUIRED portaudio-2.0 IMPORTED_TARGET)
if(TARGET PkgConfig::portaudio)
	message(STATUS "Found portaudio")
	target_link_libraries(compi PkgConfig::portaudio)
endif()

include_directories(${PROJECT_SOURCE_DIR}/external/kissfft)
include_directories(${PROJECT_SOURCE_DIR}/external/kissfft/tools)
include_directories(${PROJECT_SOURCE_DIR}/external/phash)
include_directories(${PROJECT_SOURCE_DIR}/external/log/include)
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(compi pthread snmp++ agent++ portaudio)

set_target_properties(compi PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)

#install
install(TARGETS compi RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/compi)")
install(FILES  ${PROJECT_SOURCE_DIR}/config/compi.ini DESTINATION /usr/local/etc)
