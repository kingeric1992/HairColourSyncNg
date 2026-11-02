cmake_minimum_required(VERSION 3.21)
project(HairColourSyncNg LANGUAGES CXX)

set(BUILD_TESTS OFF)
add_subdirectory(extern/CommonLibSSE-NG)

# Plain DLL: the sources define their own SKSEPlugin_Version / Query / Load exports.
add_library(${PROJECT_NAME} SHARED dllmain.cpp plugin.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE psapi)
