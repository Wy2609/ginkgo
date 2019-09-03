if(NOT DEFINED HIP_PATH)
    if(NOT DEFINED ENV{HIP_PATH})
        set(HIP_PATH "/opt/rocm/hip" CACHE PATH "Path to which HIP has been installed")
        set(ENV{HIP_PATH} ${HIP_PATH})
    else()
        set(HIP_PATH $ENV{HIP_PATH} CACHE PATH "Path to which HIP has been installed")
    endif()
endif()

if(NOT DEFINED HIPBLAS_PATH)
    if(NOT DEFINED ENV{HIPBLAS_PATH})
        set(HIPBLAS_PATH "${HIP_PATH}/../hipblas" CACHE PATH "Path to which HIPBLAS has been installed")
    else()
        set(HIPBLAS_PATH $ENV{HIPBLAS_PATH} CACHE PATH "Path to which HIPBLAS has been installed")
    endif()
endif()

if(NOT DEFINED HIPSPARSE_PATH)
    if(NOT DEFINED ENV{HIPSPARSE_PATH})
        set(HIPSPARSE_PATH "${HIP_PATH}/../hipsparse" CACHE PATH "Path to which HIPSPARSE has been installed")
    else()
        set(HIPSPARSE_PATH $ENV{HIPSPARSE_PATH} CACHE PATH "Path to which HIPSPARSE has been installed")
    endif()
endif()

if (GINKGO_HIP_PLATFORM STREQUAL "hcc" AND NOT GINKGO_HIP_AMDGPU)
    message(WARNING "GINKGO_HIP_AMDGPU is not set. All hipcc calls may be missing AMDGPU targets.")
endif()


set(CMAKE_MODULE_PATH "${HIP_PATH}/cmake" ${CMAKE_MODULE_PATH})
list(APPEND CMAKE_PREFIX_PATH
    "${HIP_PATH}/../lib/cmake"
    "${HIPBLAS_PATH}/lib/cmake"
    "${HIPSPARSE_PATH}/lib/cmake")

find_package(HIP REQUIRED)
find_package(hipblas REQUIRED)
find_package(hipsparse REQUIRED)
#TODO: find_header/whatever for THRUST


set(GINKGO_HIP_SOURCES
    base/exception.hip.cpp
    base/executor.hip.cpp
    base/version.hip.cpp
    components/zero_array.hip.cpp
    factorization/par_ilu_kernels.hip.cpp
    matrix/coo_kernels.hip.cpp
    matrix/csr_kernels.hip.cpp
    matrix/dense_kernels.hip.cpp
    matrix/ell_kernels.hip.cpp
    matrix/hybrid_kernels.hip.cpp
    matrix/sellp_kernels.hip.cpp
    matrix/sparsity_csr_kernels.hip.cpp
    preconditioner/jacobi_kernels.hip.cpp
    solver/bicgstab_kernels.hip.cpp
    solver/cg_kernels.hip.cpp
    solver/cgs_kernels.hip.cpp
    solver/fcg_kernels.hip.cpp
    solver/gmres_kernels.hip.cpp
    solver/ir_kernels.hip.cpp
    solver/lower_trs_kernels.hip.cpp
    solver/upper_trs_kernels.hip.cpp
    stop/criterion_kernels.hip.cpp
    stop/residual_norm_reduction_kernels.hip.cpp)

set(GINKGO_HIPCC_OPTIONS -fPIC ${GINKGO_HIP_COMPILER_FLAGS})


set_source_files_properties(${GINKGO_HIP_SOURCES} PROPERTIES HIP_SOURCE_PROPERTY_FORMAT TRUE)
hip_add_library(ginkgo_hip $<TARGET_OBJECTS:ginkgo_hip_device> ${GINKGO_HIP_SOURCES} HIPCC_OPTIONS ${GINKGO_HIPCC_OPTIONS})

# target_compile_options(ginkgo_hip PRIVATE -fno-gpu-rdc)
if(GINKGO_HIP_AMDGPU AND GINKGO_HIP_PLATFORM STREQUAL "hcc")
    foreach(target ${GINKGO_HIP_AMDGPU})
        target_compile_options(ginkgo_hip PRIVATE --amdgpu-target=${target})
    endforeach()
endif()

target_compile_options(ginkgo_hip PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GINKGO_COMPILER_FLAGS}>)
if(GINKGO_WITH_CLANG_TIDY AND GINKGO_CLANG_TIDY_PATH)
    set_property(TARGET ginkgo_hip PROPERTY CXX_CLANG_TIDY "${GINKGO_CLANG_TIDY_PATH};-checks=*")
endif()
if(GINKGO_WITH_IWYU AND GINKGO_IWYU_PATH)
    set_property(TARGET ginkgo_hip PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${GINKGO_IWYU_PATH})
endif()

target_link_libraries(ginkgo_hip PUBLIC hip::device PRIVATE roc::hipblas roc::hipsparse)

target_include_directories(ginkgo_hip
    PUBLIC
    /opt/rocm/include
    PRIVATE
    $<BUILD_INTERFACE:${ROCPRIM_INCLUDE_DIRS}>)

ginkgo_compile_features(ginkgo_hip)
ginkgo_default_includes(ginkgo_hip)
ginkgo_install_library(ginkgo_hip hip)

if(GINKGO_BUILD_TESTS)
    get_target_property(GINKGO_LINK_LIBRARIES ginkgo LINK_LIBRARIES)
    set(GINKGO_RPATH_FOR_HIP "-Wl,-rpath,$<TARGET_FILE_DIR:ginkgo>")
    foreach(target ${GINKGO_LINK_LIBRARIES})
        if("${target}" MATCHES "^ginkgo")
            set(GINKGO_RPATH_FOR_HIP "${GINKGO_RPATH_FOR_HIP}:$<TARGET_FILE_DIR:${target}>")
        endif()
    endforeach()

    add_subdirectory(test)
endif()