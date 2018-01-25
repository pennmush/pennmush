cmake_minimum_required(VERSION 3.6)
project(dbtools)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS iostreams)
find_program(INDENT NAMES clang-format clang-format-6.0 clang-format-5.0 clang-format-4.0 DOC "clang-format version")
include(CheckCXXCompilerFlag)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")
  check_cxx_compiler_flag(-march=native HAVE_MARCH_NATIVE)
  if(HAVE_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  endif()
endif()

add_library(dbio STATIC database.cpp io_primitives.cpp db_labelsv1.cpp
  db_oldstyle.cpp utils.cpp bits.cpp)
set_property(TARGET dbio PROPERTY CXX_STANDARD 17)
target_include_directories(dbio PRIVATE ${Boost_INCLUDES} "${CMAKE_SOURCE_DIR}/..")

add_executable(db2dot db2dot.cpp)
set_property(TARGET db2dot PROPERTY CXX_STANDARD 17)
target_include_directories(db2dot PRIVATE ${Boost_INCLUDES})
target_link_libraries(db2dot dbio ${Boost_IOSTREAMS_LIBRARY})

add_executable(dbupgrade dbupgrade.cpp)
set_property(TARGET dbupgrade PROPERTY CXX_STANDARD 17)
target_include_directories(dbupgrade PRIVATE ${Boost_INCLUDES})
target_link_libraries(dbupgrade dbio ${Boost_IOSTREAMS_LIBRARY})

add_executable(pwutil pwutil.cpp)
set_property(TARGET pwutil PROPERTY CXX_STANDARD 17)
target_include_directories(pwutil PRIVATE ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})
target_link_libraries(pwutil dbio ${OPENSSL_LIBRARIES} ${Boost_IOSTREAMS_LIBRARY})

add_custom_target(indent COMMAND ${INDENT} -i --style=file *.cpp *.h
  COMMENT "Formatting source files")

add_custom_command(OUTPUT README.html 
  COMMAND pandoc -t html5 -s --toc --toc-depth=2 -o README.html README.md
  MAIN_DEPENDENCY README.md
  COMMENT "Creating README.html")
add_custom_target(html DEPENDS README.html)

