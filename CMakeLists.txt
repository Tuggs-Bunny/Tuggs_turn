cmake_minimum_required(VERSION 3.10)
project(TuggsTurn LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(tuggs_turn
    main.cpp
    ConsoleUI.cpp
    Settings.cpp
    MouseSimulation.cpp
    CS2Monitor.cpp
    InputManager.cpp
    Utils.cpp
)

target_include_directories(tuggs_turn PRIVATE . third_party)

find_package(Threads REQUIRED)
target_link_libraries(tuggs_turn PRIVATE Threads::Threads)

# X11 is optional: with it we detect CS2 window focus (works via XWayland
# too); without it we fall back to checking that the cs2 process is running.
find_package(X11)
if(X11_FOUND)
    target_compile_definitions(tuggs_turn PRIVATE HAVE_X11)
    target_include_directories(tuggs_turn PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(tuggs_turn PRIVATE ${X11_LIBRARIES})
endif()
