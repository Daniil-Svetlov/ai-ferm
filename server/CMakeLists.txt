cmake_minimum_required(VERSION 3.10)
project(FarmDBLogger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Найти MySQL
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

# Найти mosquitto
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Найти nlohmann/json
find_package(nlohmann_json REQUIRED)

# Добавить исполняемый файл
add_executable(farm_db_logger main.cpp)

# Подключить заголовочные файлы
target_include_directories(farm_db_logger PRIVATE
    ${MYSQL_INCLUDE_DIRS}
    ${MOSQUITTO_INCLUDE_DIRS}
)

# Подключить библиотеки
target_link_libraries(farm_db_logger
    ${MYSQL_LIBRARIES}
    ${MOSQUITTO_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Флаги компиляции
target_compile_options(farm_db_logger PRIVATE -Wall -Wextra -O2)
