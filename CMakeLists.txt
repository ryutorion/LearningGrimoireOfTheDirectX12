cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_CONFIGURATION_TYPES )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DirectXTex_ROOT "" CACHE FILEPATH "Path to DirectXTex git repository")
if(NOT EXISTS "${DirectXTex_ROOT}")
	message(FATAL_ERROR "Set DirectXTex_ROOT")
endif()

project(
	LearningGrimoireOfTheDirectX12
	VERSION 0.0.1
	LANGUAGES CXX
)

if(MSVC_TOOLSET_VERSION EQUAL 142)
# Visual Studio 2019�̏ꍇ
	if(CMAKE_SYSTEM_VERSION VERSION_GREATER 10)
		# Windows 10�̏ꍇ
		set(DirectXTex_LIBPATH "${DirectXTex_ROOT}/DirectXTex/Bin/Desktop_2019_Win10/${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(DirectXTex_LIBPATH "${DirectXTex_ROOT}/DirectXTex/Bin/Desktop_2019/${CMAKE_VS_PLATFORM_NAME}")
	endif()
elseif(MSVC_TOOLSET_VERSION EQUAL 141)
# Visual Studio 2017�̏ꍇ
	if(CMAKE_SYSTEM_VERSION VERSION_GREATER 10)
		# Windows 10�̏ꍇ
		set(DirectXTex_LIBPATH "${DirectXTex_ROOT}/DirectXTex/Bin/Desktop_2017_Win10/${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(DirectXTex_LIBPATH "${DirectXTex_ROOT}/DirectXTex/Bin/Desktop_2017/${CMAKE_VS_PLATFORM_NAME}")
	endif()
else()
	message(FATAL_ERROR "Use Visual Studio 2017 or 2019")
endif()

add_executable(
	LearningGrimoireOfTheDirectX12 WIN32
	main.cpp
	renderer_dx12.h
	renderer_dx12.cpp
)

target_include_directories(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	${DirectXTex_ROOT}/DirectXTex
)

target_compile_definitions(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	WORKING_DIR="${CMAKE_CURRENT_LIST_DIR}"
)

target_link_directories(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	$<$<CONFIG:Debug>:${DirectXTex_LIBPATH}/Debug>
	$<$<CONFIG:Release>:${DirectXTex_LIBPATH}/Release>
)

target_link_libraries(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	d3d12.lib
	dxgi.lib
	d3dcompiler.lib
	DirectXTex.lib
)
