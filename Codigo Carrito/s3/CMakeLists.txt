cmake_minimum_required(VERSION 3.16)

# Componente compartido con el P4 (protocolo, enlace SPI, tabla de ajustes)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kakata_car_s3)
