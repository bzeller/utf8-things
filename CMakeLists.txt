cmake_minimum_required(VERSION 3.10)

project(Utf8Things LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Uncomment if you want to strictly adhere to the standard

# Add an executable target.
add_executable(utf8-things main.cc)

# Optional: If your main.cc includes headers from an 'include' directory
# include_directories(include)

# Optional: If your project depends on external libraries found by CMake (e.g., Boost)
# find_package(Boost REQUIRED COMPONENTS system filesystem)
# target_link_libraries(MyExecutable PRIVATE Boost::system Boost::filesystem)
