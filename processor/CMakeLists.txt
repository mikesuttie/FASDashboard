###########
# Project #
###########
cmake_minimum_required(VERSION 3.10)

project(faceScreenServer)

find_package(cpprestsdk REQUIRED NAMES cpprestsdk cpprest)


#######################
# Folder organisation #
#######################
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)




#########################
# External Dependencies #
#########################
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED) # https://newbedev.com/undefined-reference-to-boost-system-system-category-when-compiling
#find_package(Boost REQUIRED regex date_time system filesystem thread graph program_options)
find_package(Boost REQUIRED thread)

# VTK Libraries
###set(VTK_DIR $ENV{VTK_DIR})
#set(VTK_DIR "C:/VTK/VTK/buildWithMesa")

#set(VTK_DIR "/usr/local/lib")
#set(VTK_DIR "/home/i/VTK-9.0.1/buildStaticReleaseWithMesaNoX")
#set(VTK_DIR "/vtksrc/buildStaticReleaseWithMesaNoX")
set(VTK_DIR "/vtksrc/build")

find_package(VTK REQUIRED NO_MODULE)
include(${VTK_USE_FILE})


# Headers directories
include_directories(${PROJECT_SOURCE_DIR}/src		)
###########
# Targets #
###########

# Sources
set (SOURCES
	src/faceScreenServer.cpp
	src/faceScreenProcessor.cpp
	src/faceScreeningObject.cpp
	src/heatmapProcessing/msNormalisationTools.cpp
	src/heatmapProcessing/vtkSurfacePCA.cpp
	src/mathUtils/C2dVector.cpp
	src/mathUtils/C3dVector.cpp
	src/subjectClassification/classificationTools.cpp
	src/subjectClassification/CFloatMatrix.cpp
	src/PFLcomputation/msPFLMeasure.cpp
	src/utils/tooJpeg/toojpeg.cpp
	src/utils/yaml/Yaml.cpp
	src/BellusUtils/landmarkProcessing.cpp
)

add_executable(faceScreenServer ${SOURCES})

set_target_properties(faceScreenServer PROPERTIES
            CXX_STANDARD 17
	    CXX_EXTENSIONS OFF
            )

#set (CMAKE_CXX_STANDARD 17)
#set (CMAKE_CXX_STANDARD_REQUIRED ON) # Causes Cmake error if c++17 is not supported, rather than compiler or linker error.
###set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
#set(CMAKE_CXX_COMPILER /usr/bin/g++-8)
#set(CMAKE_C_COMPILER /usr/bin/gcc-8)

target_link_libraries(faceScreenServer PRIVATE ${VTK_LIBRARIES} ${Boost_LIBRARIES} cpprestsdk::cpprest stdc++fs)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(faceScreenServer PUBLIC OpenMP::OpenMP_CXX)
endif()


# Needed since vtk 8.9 - or factory methods will fail
vtk_module_autoinit(
    TARGETS faceScreenServer
    MODULES ${VTK_LIBRARIES}
    )

