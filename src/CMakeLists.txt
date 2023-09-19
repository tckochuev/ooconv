set(TARGET_NAME "${PROJECT_NAME}")
set(HEADERS)
set(SOURCES main.cpp)

if(UNIX)
    set(CMAKE_BUILD_RPATH $ORIGIN)
endif()
add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

add_compile_definitions(_HAS_AUTO_PTR_ETC=1)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.62.0 REQUIRED COMPONENTS program_options filesystem)

if(WIN32)

set(
    DOCUMENT_BUILDER_ROOT
    "${CMAKE_SOURCE_DIR}/lib/DocumentBuilder/win"
)
set(
    DOCUMENT_BUILDER_LIBRARY_NAME
    "doctrenderer.lib"
)

elseif(UNIX)

set(
    DOCUMENT_BUILDER_ROOT
    "${CMAKE_SOURCE_DIR}/lib/DocumentBuilder/linux"
)
set(
    DOCUMENT_BUILDER_LIBRARY_NAME
    "libdoctrenderer.so"
)

endif()

set(
    DOCUMENT_BUILDER_INCLUDE_DIRS 
    "${DOCUMENT_BUILDER_ROOT}/include"
)
set(
    DOCUMENT_BUILDER_LIBRARIES 
    "${DOCUMENT_BUILDER_ROOT}/${DOCUMENT_BUILDER_LIBRARY_NAME}"
)
#relative to executable
add_compile_definitions(DOCUMENT_BUILDER_WORKING_DIRECTORY=".")

target_include_directories(
    ${TARGET_NAME}
    PRIVATE 
    ${Boost_INCLUDE_DIRS}
    ${DOCUMENT_BUILDER_INCLUDE_DIRS}
)
target_link_libraries(
    ${TARGET_NAME} 
    PRIVATE
    ${Boost_LIBRARIES}
    ${DOCUMENT_BUILDER_LIBRARIES}
)

#file(GLOB DOCUMENT_BUILDER_DLLS "${DOCUMENT_BUILDER_ROOT}/*.dll")
#file(GLOB DOCUMENT_BUILDER_WORKING_DIRECTORY_CONTENT "${DOCUMENT_BUILDER_ROOT}/*")#"${DOCUMENT_BUILDER_ROOT}/^(?!(include|README|LICENSE|unins)).*$")

add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${DOCUMENT_BUILDER_ROOT}"
    $<TARGET_FILE_DIR:${TARGET_NAME}>
)
