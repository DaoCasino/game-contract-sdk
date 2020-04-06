# Example contracts

## Default contract directoties structure
```
example-game
│   CMakeLists.txt
│
└───contracts
│   │   CMakeLists.txt
│   │   
│   └───include/example-game/
│   │   │   example-game.hpp
│   │   │   ...
│   │
│   └───src
│       │   example-game.cpp
│       │   ...
│   
└───tests
    │   CMakeLists.txt
    │   contracts.hpp.in
    │   example-game_tests.cpp
    │   ...
```

## How to build own contract
 - make similar project structure as `stub` example
 - adapt all `CMakeLists.txt` for your game
 - add this sdk as submodule(get all latest release tag)
 - put correct `GAME_SDK_PATH` to main `CMakeLists.txt`
 - implement your game logic