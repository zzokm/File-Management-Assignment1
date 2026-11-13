cmake_minimum_required(VERSION 3.15)
project(HealthcareManagementSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Core System Headers & Sources
set(HEADERS
    include/Models.h
    include/StorageEngine.h
    include/IndexEngine.h
    include/BusinessLogic.h
    include/QueryEngine.h
)

set(SOURCES
    src/StorageEngine.cpp
    src/IndexEngine.cpp
    src/BusinessLogic.cpp
    src/QueryEngine.cpp
)

add_library(HealthcareSystem STATIC
    ${SOURCES}
    ${HEADERS}
)

add_executable(HealthcareApp
    src/main.cpp
)

target_link_libraries(HealthcareApp PRIVATE HealthcareSystem)

