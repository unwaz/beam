cmake_minimum_required(VERSION 3.5)

set (TARGET_NAME mnemonic)

find_package(Boost)

set(MNEMONIC_SRC
    dictionary.cpp
    mnemonic.cpp
)

add_library(${TARGET_NAME} STATIC ${MNEMONIC_SRC})
target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_SOURCE_DIR}}/external)
target_link_libraries(${TARGET_NAME}
    PRIVATE 
        core
        Boost::boost
        pbkdf


)

add_subdirectory(unittests)
