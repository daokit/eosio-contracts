project(dao)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( dao dao dao.cpp )
target_include_directories( dao PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( dao ${CMAKE_SOURCE_DIR}/../ricardian )