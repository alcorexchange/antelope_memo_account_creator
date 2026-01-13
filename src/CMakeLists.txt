project(memo_acc_creator)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( memo_acc_creator memo_acc_creator memo_acc_creator.cpp )
target_include_directories( memo_acc_creator PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( memo_acc_creator ${CMAKE_SOURCE_DIR}/../ricardian )