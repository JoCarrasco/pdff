cmake_minimum_required(VERSION 4.1)
project(pdff)

find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

set(MUPDF_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib")
# 3. Find the libraries

find_library(MUPDF_LIB mupdf HINTS ${MUPDF_LIB_PATH})
find_library(MUPDF_THIRD_LIB mupdf-third HINTS ${MUPDF_LIB_PATH})

# 4. Check if they were actually found to give a better error message
if(NOT MUPDF_LIB OR NOT MUPDF_THIRD_LIB)
    message(FATAL_ERROR "MuPDF libraries not found in ${MUPDF_LIB_PATH}.
    Ensure libmupdf.a and libmupdf-third.a are in that folder.")
endif()

add_executable(pdff src/main.cpp
        src/core.cpp
        src/core.h)

# 5. Include your local headers
target_include_directories(pdff PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 6. Link everything
target_link_libraries(pdff
        PRIVATE
        ${MUPDF_LIB}
        ${MUPDF_THIRD_LIB}
        SDL2::SDL2        # <--- Add this
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
)