cmake_minimum_required(VERSION 3.7)
project(judger)

set(CMAKE_CXX_STANDARD 11)
set(
        SOURCE_FILES
        src/main.cpp
        src/Runner.cpp
        src/Runner.h
        src/RunResult.h
        src/RunResult.cpp
        src/Config.cpp
        src/Config.h
        src/Utils.cpp
        src/Utils.h
        src/Submit.cpp
        src/Submit.h
        src/Exception.h
        src/DatabaseHandler.cpp
        src/DatabaseHandler.h
)

include_directories(src/include)

link_directories(/usr/local/lib)

add_executable(judger ${SOURCE_FILES})

target_link_libraries(judger pthread)
target_link_libraries(judger mysqlclient)
