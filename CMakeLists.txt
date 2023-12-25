cmake_minimum_required(VERSION 3.9)

project(TCC LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

file(GLOB SOURCES "src/common/*.cpp" "src/common/*.c")
file(GLOB HEADERS "src/common/*.hpp" "src/common/*.h")
file(GLOB TCC_SOURCES "src/tcc/*.cpp")
file(GLOB TCC_HEADERS "src/tcc/*.hpp")
#file(GLOB TLD_SOURCES "src/ld/*.cpp")
#file(GLOB TLD_HEADERS "src/ld/*.hpp")

if(NOT MSVC)
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie -Wl,--as-needed,--gc-sections")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -fno-rtti -fno-plt -Wall -pedantic -Wextra -Wunused-result -fdata-sections -ffunction-sections -fvisibility=hidden")
else()
        set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /W3 /GR- /EHsc /Gw")
endif()

include_directories(src/)
add_executable(tcc ${SOURCES} ${HEADERS} ${TCC_SOURCES} ${TCC_HEADERS})
#add_executable(tld ${SOURCES} ${HEADERS} ${TLD_SOUECES} ${TLD_HEADERS}find_package(png REQUIRED)
find_package(PNG REQUIRED)
target_link_libraries(tcc ${PNG_LIBRARY_RELEASE})
#target_link_libraries(tld ${PNG_LIBRARY_RELEASE})
