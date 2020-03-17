add_library(platform-contracts INTERFACE)

target_include_directories(platform-contracts INTERFACE
    platform-contracts/contracts/platform/include/
    platform-contracts/contracts/casino/include/
    platform-contracts/contracts/events/include/
)
