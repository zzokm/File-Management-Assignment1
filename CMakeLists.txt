cmake_minimum_required(VERSION 3.15)
project(HealthcareManagementSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Core System Headers & Sources (Role 1 & Role 2)
set(HEADERS
    include/Models.h
    include/StorageEngine.h
    include/IndexEngine.h
)

set(SOURCES
    src/StorageEngine.cpp
    src/IndexEngine.cpp
)

add_library(HealthcareSystem STATIC
    ${SOURCES}
    ${HEADERS}
)

