cmake_minimum_required(VERSION 3.16)

# Componente compartido con el S3 (protocolo 433, enlace SPI, tabla de ajustes)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kakata_car_p4)
