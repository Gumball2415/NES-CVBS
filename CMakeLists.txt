# CMakeList.txt : CMake project for NES-CVBS, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.21)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (NES-CVBS-Demo
	VERSION 0.0.0.0
	DESCRIPTION "NES emulator NTSC/PAL composite encoder/decoder."
	HOMEPAGE_URL "https://github.com/gumball2415/nes-cvbs"
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

if (SANITIZE_ADDRESS)
	add_compile_options(/fsanitize=address)
	add_link_options(/fsanitize=address)
endif ()

# lodepng
add_subdirectory ("src/lodepng")
list (APPEND EXTRA_LIBS "lodepng")
list (APPEND EXTRA_INCLUDES "${PROJECT_BINARY_DIR}/lodepng")

# SDL2
find_package (SDL2 REQUIRED IMPORTED)
list (APPEND EXTRA_LIBS ${SDL2_LIBRARIES})
list (APPEND EXTRA_INCLUDES ${SDL2_INCLUDE_DIRS})

add_executable (NES-CVBS-Demo "main.cpp" "main.h" "src/NES-CVBS.cpp" "src/NES-CVBS.h" "src/PPUTimings.h" "src/PPUVoltages.h")

target_include_directories (NES-CVBS-Demo
	PUBLIC "${PROJECT_BINARY_DIR}"
	${EXTRA_INCLUDES})
	
target_link_libraries (NES-CVBS-Demo PUBLIC ${EXTRA_LIBS})

# TODO: Add tests and install targets if needed.
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NES-CVBS-Demo PROPERTY CXX_STANDARD 23)
endif()

if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
add_custom_command(TARGET NES-CVBS-Demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:NES-CVBS-Demo>)
else ()
add_custom_command(TARGET NES-CVBS-Demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/lib/x86/SDL2.dll"
        $<TARGET_FILE_DIR:NES-CVBS-Demo>)
endif ()
