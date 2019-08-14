cmake_minimum_required(VERSION 3.11)

add_definitions( -DSQLITE_HAS_CODEC)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

if( OpenSSL_FOUND )
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "error, OpenSSL not found, please install: 'apt-get install libssl-dev' on Ubuntu")
endif()

set(SQLITE_SRC
    sqlite3.c
)

add_library(sqlite STATIC ${SQLITE_SRC})

if(UNIX AND NOT ANDROID)
    target_link_libraries(sqlite PRIVATE dl pthread)
endif()

target_link_libraries(sqlite PRIVATE OpenSSL::Crypto)

if(ANDROID)
    # On Android there's no tmp partition, so always use a MEMORY temp store.
    add_definitions(-DSQLITE_TEMP_STORE=3)
endif()