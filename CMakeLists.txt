cmake_minimum_required(VERSION 3.14.5)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(flare LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FLARE_PYTHON_VERSION "" CACHE STRING "Python version to use for compiling modules")

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "[flare] CMAKE_BUILD_TYPE not specified, setting it to "
                 "Release. Use `-DCMAKE_BUILD_TYPE=...` to overwrite.")
  set(CMAKE_BUILD_TYPE Release)
endif()

#
# Dependencies
#

include(FetchContent)

# googletest
################################################################################

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/External/googletest
    UPDATE_COMMAND ""
)
FetchContent_MakeAvailable(googletest)

# Json
################################################################################

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/External/json
    GIT_TAG v3.9.1)

FetchContent_GetProperties(json)
FetchContent_Populate(json)
add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})
include_directories(${json_SOURCE_DIR})

# Kokkos
################################################################################

#FetchContent_Declare(kokkos
#    GIT_REPOSITORY https://github.com/kokkos/kokkos
#    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/External/kokkos
#    UPDATE_COMMAND ""
#)
#FetchContent_GetProperties(kokkos)
#FetchContent_Populate(kokkos)
#add_subdirectory(${kokkos_SOURCE_DIR} ${kokkos_BINARY_DIR})
#include_directories(${Kokkos_SOURCE_DIR})


# Eigen3
################################################################################

ExternalProject_Add(
    eigen_project
    SOURCE_DIR "${CMAKE_BINARY_DIR}/External/Eigen3"
    URL "https://github.com/eigenteam/eigen-git-mirror/archive/3.3.7.tar.gz"
    URL_HASH MD5=77a2c934eaf35943c43ee600a83b72df
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(eigen_project SOURCE_DIR)
add_library(Eigen3 INTERFACE)
target_include_directories(Eigen3 SYSTEM INTERFACE ${SOURCE_DIR})
include_directories(${SOURCE_DIR})

# pybind11
###############################################################################
ExternalProject_Add(
    pybind11_project
    SOURCE_DIR "${CMAKE_BINARY_DIR}/External/pybind11"
    URL "https://github.com/pybind/pybind11/archive/v2.3.0.tar.gz"
    URL_HASH MD5=e2120c5b0e3c20a93f2dfcdc55026ba8
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
if (NOT FLARE_PYTHON_VERSION)
    set(Python_ADDITIONAL_VERSIONS 3.7 3.6 3.5)
endif()
find_package(PythonLibsNew ${FLARE_PYTHON_VERSION} REQUIRED)

add_library(pybind11 INTERFACE)
ExternalProject_Get_Property(pybind11_project SOURCE_DIR)
target_include_directories(pybind11 SYSTEM INTERFACE ${SOURCE_DIR}/include)
target_include_directories(pybind11 SYSTEM INTERFACE ${PYTHON_INCLUDE_DIRS})

if(APPLE)
      TARGET_LINK_LIBRARIES(pybind11 INTERFACE "-undefined dynamic_lookup")
      message(STATUS "Building in conda environment on MAC")
else()
      target_link_libraries(pybind11 INTERFACE ${PYTHON_LIBRARIES})
endif()

# Greatly reduces the code bloat
target_compile_options(pybind11 INTERFACE "-fvisibility=hidden")
###############################################################################

# Specify source files.
set(FLARE_SOURCES
    src/flare_pp/y_grad.cpp
    src/flare_pp/radial.cpp
    src/flare_pp/cutoffs.cpp
    src/flare_pp/structure.cpp
    src/flare_pp/bffs/sparse_gp.cpp
    src/flare_pp/bffs/gp.cpp
    src/flare_pp/descriptors/descriptor.cpp
    src/flare_pp/descriptors/b2.cpp
    src/flare_pp/descriptors/b2_norm.cpp
    src/flare_pp/descriptors/b2_simple.cpp
    src/flare_pp/descriptors/b3.cpp
    src/flare_pp/descriptors/wigner3j.cpp
    src/flare_pp/descriptors/two_body.cpp
    src/flare_pp/descriptors/three_body.cpp
    src/flare_pp/descriptors/three_body_wide.cpp
    src/flare_pp/descriptors/four_body.cpp
    src/flare_pp/kernels/kernel.cpp
    src/flare_pp/kernels/normalized_dot_product.cpp
    src/flare_pp/kernels/norm_dot_icm.cpp
    src/flare_pp/kernels/squared_exponential.cpp
    src/exceptions.cpp
    src/algebra/Matrix.cpp
    src/algebra/PMatrix.cpp
    src/algebra/SMatrix.cpp
    src/mpi/mpiController.cpp
    src/mpi/mpiHelper.cpp)

set(PYBIND_SOURCES
    src/ace_binding.cpp)

# Include sources.
include_directories(
  src
  src/flare_pp
  src/flare_pp/descriptors
  src/flare_pp/kernels
  src/flare_pp/bffs
  src/algebra
  src/mpi)

# Create flare library.
# Should be static to avoid issues with pip installation.
# See: https://github.com/pybind/cmake_example/issues/11
add_library(flare STATIC ${FLARE_SOURCES})
set_target_properties(flare PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(flare eigen_project)

################ PARALLEL ####################
# build with MPI and scalapack - adapted from Phoebe's CMakeLists.txt
if (MPI_AVAIL)
  find_package(MPI REQUIRED)
  target_link_libraries(flare MPI::MPI_CXX)
  add_definitions("-DMPI_AVAIL") # this sets turns on ifdefs for mpi in the cpp

  find_library(SCALAPACK_LIB NAMES scalapack scalapack-openmpi mkl_scalapack_lp64 PATHS ENV LD_LIBRARY_PATH)
  if(${SCALAPACK_LIB} MATCHES NOTFOUND)
      ExternalProject_Add(scalapack_dep
          BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/scalapack_build
          SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/scalapack_src
          GIT_REPOSITORY https://github.com/Reference-ScaLAPACK/scalapack.git
          INSTALL_COMMAND ""
          UPDATE_COMMAND ""
          CMAKE_ARGS
              -DCMAKE_BUILD_TYPE=Release
              -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
              -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
              -DCMAKE_Fortran_COMPILER=${CMAKE_Fortran_COMPILER}
          )
      set(SCALAPACK_LIB scalapack)
      message(WARNING "No SCALAPACK library found in CMAKE_LIBRARY_PATH, building our own.")
      add_dependencies(flare scalapack_dep)
  else()
      if(${SCALAPACK_LIB} MATCHES mkl)
          if("${MPI_CXX_LIBRARIES}" MATCHES openmpi)
              find_library(BLACS_LIB NAMES mkl_blacs_openmpi_lp64 PATHS ENV LD_LIBRARY_PATH)
          elseif("${MPI_CXX_LIBRARIES}" MATCHES intel)
              find_library(BLACS_LIB NAMES mkl_blacs_intelmpi_lp64 PATHS ENV LD_LIBRARY_PATH)
          else()
              message(FATAL_ERROR "Confused by MPI library when looking for BLACS.")
          endif()
          if(${BLACS_LIB} MATCHES NOTFOUND)
              message(FATAL_ERROR "Found Intel SCALAPACK but not BLACS")
          endif()
          message(STATUS "Found BLACS: ${BLACS_LIB}")
      endif()
      message(STATUS "Found SCALAPACK: ${SCALAPACK_LIB}")
  endif()

  find_package(BLAS REQUIRED)
  find_package(LAPACK REQUIRED)
  include_directories(${BLAS_INCLUDE_DIR})
  include_directories(${LAPACK_INCLUDE_DIR})
  ## Very important to link scalapack before blas and lapack
  target_link_libraries(flare ${SCALAPACK_LIB} ${BLACS_LIB} ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES} -lgfortran)

else()

  ## even for the serial version, we require lapack
  find_package(BLAS REQUIRED)
  find_package(LAPACK REQUIRED)
  include_directories(${BLAS_INCLUDE_DIR})
  include_directories(${LAPACK_INCLUDE_DIR})
  target_link_libraries(flare ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES} -lgfortran)

endif()

# Link to json.
target_link_libraries(flare PUBLIC nlohmann_json::nlohmann_json)

# Link to OpenMP.
if (DEFINED ENV{OMP_LOC})
    target_link_libraries(flare PUBLIC $ENV{OMP_LOC})
else()
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(flare PUBLIC OpenMP::OpenMP_CXX)
    endif()
endif()

# Check for user-specified MKL package.
if (DEFINED ENV{MKL_INCLUDE} AND DEFINED ENV{MKL_LIBS})
  message(STATUS "Linking Eigen to user-specified MKL libraries.")
  include_directories($ENV{MKL_INCLUDE})
  target_link_libraries(flare PUBLIC $ENV{MKL_LIBS})
  target_compile_definitions(flare PUBLIC EIGEN_USE_MKL_ALL=1)
else()
  find_package(LAPACK REQUIRED)
  target_link_libraries(flare PUBLIC ${LAPACK_LIBRARIES})

  if (LAPACK_LIBRARIES MATCHES mkl)
    message(STATUS "Linking Eigen to MKL.")
    target_compile_definitions(flare PUBLIC EIGEN_USE_MKL_ALL=1)
  else()
    find_package(LAPACKE)
    if(LAPACKE_FOUND)
      target_link_libraries(flare PUBLIC ${LAPACKE_LIBRARIES})
    endif()
    message(STATUS "Linking Eigen to LAPACKE.")
    target_compile_definitions(flare PUBLIC EIGEN_USE_LAPACKE=1)
  endif()
endif()

# Create pybind module.
add_library(flare_module SHARED ${PYBIND_SOURCES})
target_link_libraries(flare_module PUBLIC flare pybind11)
set_target_properties(flare_module PROPERTIES PREFIX "${PYTHON_MODULE_PREFIX}"
                                              SUFFIX "${PYTHON_MODULE_EXTENSION}")
add_dependencies(flare_module pybind11_project)
set_target_properties(flare_module PROPERTIES OUTPUT_NAME "_C_flare")

# Add test directory.
add_subdirectory(tests)
