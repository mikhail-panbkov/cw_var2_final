cmake_minimum_required(VERSION 3.12)
project(cw2026_bplustree_dbms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cwdb src/cwdb.cpp)
add_executable(cwdb-server src/cwdb_server.cpp)
add_executable(cwdb-client src/cwdb_client.cpp)

if(WIN32)
    target_link_libraries(cwdb-server PRIVATE ws2_32)
    target_link_libraries(cwdb-client PRIVATE ws2_32)
endif()
