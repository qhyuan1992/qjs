cmake_minimum_required(VERSION 3.23)
project(qjs_learn)

set(CMAKE_CXX_STANDARD 14)

include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib/quickjs)

add_executable(qjs_learn
        main.cpp)

target_link_libraries(qjs_learn
        quickjs
        )