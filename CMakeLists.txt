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
)
