add_library(ridics_lib STATIC
    datastructures/node.cc
    resp/server.cc
)

target_link_libraries(ridics_lib
    PUBLIC
)

target_include_directories(ridics_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)