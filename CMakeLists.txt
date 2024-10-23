cmake_minimum_required(VERSION 3.28)

project(vulkan
        VERSION 0.0.1
        LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#find msvc dir, totally mess
set(MSVC_DIR ${CMAKE_CXX_COMPILER})
get_filename_component(MSVC_DIR ${MSVC_DIR} DIRECTORY)
get_filename_component(MSVC_DIR ${MSVC_DIR} DIRECTORY)
get_filename_component(MSVC_DIR ${MSVC_DIR} DIRECTORY)
get_filename_component(MSVC_DIR ${MSVC_DIR} DIRECTORY)

message(STATUS "Msvc Current Dir: ${MSVC_DIR}")

set(MSVC_MODULE_DIR ${MSVC_DIR}/modules)
file(COPY ${MSVC_MODULE_DIR} DESTINATION
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(RES_DIR properties)
set(SRC_DIR src)
set(INCLUDES include)
set(LIB_DIR lib)
set(BUILD_DIR build)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/${CMAKE_BUILD_TYPE})

set(EXE_DIR_RELA ${BUILD_DIR}/${CMAKE_BUILD_TYPE})
set(EXE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${EXE_DIR_RELA})

add_compile_options("/W4")
add_compile_options("/wd4244")
add_compile_options("/wd4100")
add_compile_options("/wd4458")
add_compile_options("/wd4267")
#add_compile_options("/wd4996")
#add_compile_options("/wd4189")
add_compile_options("/wd5030")
#add_compile_options("/g")
add_compile_options("/permissive-")
add_compile_options("/constexpr:steps4194304")
add_link_options("/NODEFAULTLIB:library")
#add_link_options("/Md")

message(STATUS "Loading...")

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/${RES_DIR}\")

    add_compile_definitions(DEBUG_CHECK=1)
else()
    add_compile_definitions(DEBUG_CHECK=0)
endif()

# Source Files...
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.c
)

# Header Files...
file(GLOB_RECURSE CURRENT_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.hpp
)

# Modules...
#
file(GLOB_RECURSE modules_files
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.ixx
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*.ixx
)

message(STATUS "Building...")


# Build .exe
add_executable(
        ${PROJECT_NAME}      #[[Project Name]]
        ${SOURCE_FILES}      #[[.cpp]]
        ${CURRENT_HEADERS}   #[[.h]]
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDES})

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/${LIB_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw3.lib
    vulkan-1.lib
    shaderc_util.lib
    freetype.lib
    shaderc_shared.lib
)

target_sources(${PROJECT_NAME} PRIVATE
    FILE_SET modules_files_set TYPE CXX_MODULES FILES ${modules_files}
)


if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    message(STATUS "Copy Properties Setup")

    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${RES_DIR}/ ${EXE_DIR}
    )
endif()


#set(FILE_TO_DELETE "${EXE_DIR}/${PROJECT_NAME}.pdb")
#add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
#    COMMAND ${CMAKE_COMMAND} -E remove ${FILE_TO_DELETE}
#    COMMENT "Deleting ${FILE_TO_DELETE}"
#)