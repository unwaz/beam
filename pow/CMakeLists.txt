cmake_minimum_required(VERSION 3.5)

#set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_MODULE_PATH}")

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

set(POW_SRC
    equihash.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/crypto/equihash_impl.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/arith_uint256.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/uint256.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/utilstrencodings.cpp
)

if(UNIX)
    set_source_files_properties(impl/crypto/equihash_impl.cpp PROPERTIES COMPILE_FLAGS -O2)
endif()

add_library(pow STATIC ${POW_SRC})

target_compile_definitions(pow PRIVATE ENABLE_MINING)

target_link_libraries(pow PRIVATE crypto)
target_link_libraries(pow PRIVATE Boost::boost)

target_include_directories(pow PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/crypto)

add_library(external_pow STATIC
    external_pow_stub.cpp
    stratum.cpp
    stratum_server.cpp
)

target_link_libraries(external_pow PRIVATE p2p pow core OpenSSL::SSL)

add_executable(miner_client miner_client.cpp)
target_link_libraries(miner_client PRIVATE external_pow Boost::boost)

add_subdirectory(unittests)
