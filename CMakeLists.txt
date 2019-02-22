cmake_minimum_required(VERSION 3.10)
project(image-uploader-cpp)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system)

file(GLOB_RECURSE IMGUP_CPP_SRC src/*.cpp src/*.hpp)

add_executable(image-uploader-cpp ${IMGUP_CPP_SRC})
target_compile_options(image-uploader-cpp PUBLIC -std=c++17 -Wall)
target_include_directories(image-uploader-cpp PUBLIC src)
target_link_libraries(image-uploader-cpp ${Boost_LIBRARIES})