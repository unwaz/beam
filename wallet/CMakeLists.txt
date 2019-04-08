cmake_minimum_required(VERSION 3.5)

set(TARGET_NAME beam-wallet${BEAM_TARGET_SUFFIX})

find_package(Boost REQUIRED)

add_definitions( -DSQLITE_HAS_CODEC )

set(WALLET_SRC
    common.cpp
    default_peers.cpp
    wallet.h
    wallet.cpp
    wallet_transaction.cpp
    wallet_network.cpp
    wallet_db.cpp
    wallet_client.h
    wallet_client.cpp
    swap_transaction.cpp
    secstring2.cpp
    unittests/util.cpp
)

add_library(wallet STATIC ${WALLET_SRC})
target_link_libraries(wallet
    PUBLIC Boost::boost
    PRIVATE core p2p sqlite
)

add_executable(${TARGET_NAME} cli.cpp)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external)

if(LINUX)
	target_link_libraries(${TARGET_NAME} -static-libstdc++ -static-libgcc)
endif()

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${TARGET_NAME} PRIVATE wallet utility)

add_library(wallet_api_proto STATIC api.cpp)
target_link_libraries(wallet_api_proto PRIVATE wallet utility http)

set(WALLET_API_NAME wallet-api${BEAM_TARGET_SUFFIX})

add_executable(${WALLET_API_NAME} api_cli.cpp)
target_include_directories(${WALLET_API_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${WALLET_API_NAME} PRIVATE wallet utility wallet_api_proto)

if(LINUX)
    target_link_libraries(${WALLET_API_NAME} PRIVATE -static-libstdc++ -static-libgcc)
endif()

add_subdirectory(unittests)

if (BEAM_SIGN_PACKAGE AND WIN32)
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${SIGNTOOL_EXECUTABLE} sign ${SIGNTOOL_OPTIONS} "${CMAKE_CURRENT_BINARY_DIR}\\{TARGET_NAME}.exe"
    )
    add_custom_command(
        TARGET ${WALLET_API_NAME} POST_BUILD
        COMMAND ${SIGNTOOL_EXECUTABLE} sign ${SIGNTOOL_OPTIONS} "${CMAKE_CURRENT_BINARY_DIR}\\${WALLET_API_NAME}.exe"
    )
endif()