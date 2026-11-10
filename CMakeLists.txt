cmake_minimum_required(VERSION 3.15)
project(HealthcareManagementSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Role 1: Storage Engine (Yehia) & Shared Models
set(HEADERS
    include/Models.h
    include/StorageEngine.h
)

set(SOURCES
    src/StorageEngine.cpp
)

add_library(HealthcareSystem STATIC
    ${SOURCES}
    ${HEADERS}
)

