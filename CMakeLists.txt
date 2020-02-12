cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(
	LearningGrimoireOfTheDirectX12
	VERSION 0.0.1
	LANGUAGES CXX
)

add_executable(
	LearningGrimoireOfTheDirectX12 WIN32
	main.cpp
	renderer_dx12.h
	renderer_dx12.cpp
)

target_link_libraries(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	d3d12.lib
	dxgi.lib
	d3dcompiler.lib
)

target_compile_definitions(
	LearningGrimoireOfTheDirectX12
	PRIVATE
	WORKING_DIR="${CMAKE_CURRENT_LIST_DIR}"
)
