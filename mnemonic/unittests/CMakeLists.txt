if (NOT Boost_FOUND)
    find_package(Boost)
endif()

add_test_snippet(bip39_test mnemonic Boost::boost)
