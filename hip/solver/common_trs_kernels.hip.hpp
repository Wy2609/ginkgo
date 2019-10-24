/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2019, the Ginkgo authors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************<GINKGO LICENSE>*******************************/

#ifndef GKO_HIP_SOLVER_COMMON_TRS_KERNELS_HIP_HPP_
#define GKO_HIP_SOLVER_COMMON_TRS_KERNELS_HIP_HPP_


#include <functional>
#include <memory>


#include <hip/hip_runtime.h>
#include <hipsparse.h>


#include <ginkgo/core/base/exception_helpers.hpp>
#include <ginkgo/core/base/math.hpp>


#include "core/matrix/dense_kernels.hpp"
#include "core/synthesizer/implementation_selection.hpp"
#include "hip/base/hipsparse_bindings.hip.hpp"
#include "hip/base/device_guard.hip.hpp"
#include "hip/base/math.hip.hpp"
#include "hip/base/pointer_mode_guard.hip.hpp"
#include "hip/base/types.hip.hpp"


namespace gko {
namespace kernels {
namespace hip {
namespace {


void should_perform_transpose_kernel(std::shared_ptr<const HipExecutor> exec,
                                     bool &do_transpose)
{
#if (defined(HIP_VERSION) && (HIP_VERSION >= 9020))


    do_transpose = false;


#elif (defined(HIP_VERSION) && (HIP_VERSION < 9020))


    do_transpose = true;


#endif
}


void init_struct_kernel(std::shared_ptr<const HipExecutor> exec,
                        std::shared_ptr<solver::SolveStruct> &solve_struct)
{
    solve_struct = std::make_shared<solver::SolveStruct>();
}


template <typename ValueType, typename IndexType>
void generate_kernel(std::shared_ptr<const HipExecutor> exec,
                     const matrix::Csr<ValueType, IndexType> *matrix,
                     solver::SolveStruct *solve_struct,
                     const gko::size_type num_rhs, bool is_upper)
{
    if (hipsparse::is_supported<ValueType, IndexType>::value) {
        auto handle = exec->get_hipsparse_handle();
        if (is_upper) {
            GKO_ASSERT_NO_HIPSPARSE_ERRORS(hipsparseSetMatFillMode(
                solve_struct->factor_descr, HIPSPARSE_FILL_MODE_UPPER));
        }


#if (defined(HIP_VERSION) && (HIP_VERSION >= 9020))


        ValueType one = 1.0;

        {
            hipsparse::pointer_mode_guard pm_guard(handle);
            hipsparse::buffer_size_ext(
                handle, solve_struct->algorithm,
                HIPSPARSE_OPERATION_NON_TRANSPOSE, HIPSPARSE_OPERATION_TRANSPOSE,
                matrix->get_size()[0], num_rhs,
                matrix->get_num_stored_elements(), &one,
                solve_struct->factor_descr, matrix->get_const_values(),
                matrix->get_const_row_ptrs(), matrix->get_const_col_idxs(),
                nullptr, num_rhs, solve_struct->solve_info,
                solve_struct->policy, &solve_struct->factor_work_size);

            // allocate workspace
            if (solve_struct->factor_work_vec != nullptr) {
                exec->free(solve_struct->factor_work_vec);
            }
            solve_struct->factor_work_vec =
                exec->alloc<void *>(solve_struct->factor_work_size);

            hipsparse::csrsm2_analysis(
                handle, solve_struct->algorithm,
                HIPSPARSE_OPERATION_NON_TRANSPOSE, HIPSPARSE_OPERATION_TRANSPOSE,
                matrix->get_size()[0], num_rhs,
                matrix->get_num_stored_elements(), &one,
                solve_struct->factor_descr, matrix->get_const_values(),
                matrix->get_const_row_ptrs(), matrix->get_const_col_idxs(),
                nullptr, num_rhs, solve_struct->solve_info,
                solve_struct->policy, solve_struct->factor_work_vec);
        }


#elif (defined(HIP_VERSION) && (HIP_VERSION < 9020))


        {
            hipsparse::pointer_mode_guard pm_guard(handle);
            hipsparse::csrsm_analysis(
                handle, HIPSPARSE_OPERATION_NON_TRANSPOSE, matrix->get_size()[0],
                matrix->get_num_stored_elements(), solve_struct->factor_descr,
                matrix->get_const_values(), matrix->get_const_row_ptrs(),
                matrix->get_const_col_idxs(), solve_struct->solve_info);
        }


#endif


    } else {
        GKO_NOT_IMPLEMENTED;
    }
}


template <typename ValueType, typename IndexType>
void solve_kernel(std::shared_ptr<const HipExecutor> exec,
                  const matrix::Csr<ValueType, IndexType> *matrix,
                  const solver::SolveStruct *solve_struct,
                  matrix::Dense<ValueType> *trans_b,
                  matrix::Dense<ValueType> *trans_x,
                  const matrix::Dense<ValueType> *b,
                  matrix::Dense<ValueType> *x)
{
    using vec = matrix::Dense<ValueType>;
    if (hipsparse::is_supported<ValueType, IndexType>::value) {
        ValueType one = 1.0;
        auto handle = exec->get_hipsparse_handle();


#if (defined(HIP_VERSION) && (HIP_VERSION >= 9020))


        x->copy_from(gko::lend(b));
        {
            hipsparse::pointer_mode_guard pm_guard(handle);
            hipsparse::csrsm2_solve(
                handle, solve_struct->algorithm,
                HIPSPARSE_OPERATION_NON_TRANSPOSE, HIPSPARSE_OPERATION_TRANSPOSE,
                matrix->get_size()[0], b->get_stride(),
                matrix->get_num_stored_elements(), &one,
                solve_struct->factor_descr, matrix->get_const_values(),
                matrix->get_const_row_ptrs(), matrix->get_const_col_idxs(),
                x->get_values(), b->get_stride(), solve_struct->solve_info,
                solve_struct->policy, solve_struct->factor_work_vec);
        }


#elif (defined(HIP_VERSION) && (HIP_VERSION < 9020))


        {
            hipsparse::pointer_mode_guard pm_guard(handle);
            if (b->get_stride() == 1) {
                auto temp_b = const_cast<ValueType *>(b->get_const_values());
                hipsparse::csrsm_solve(
                    handle, HIPSPARSE_OPERATION_NON_TRANSPOSE,
                    matrix->get_size()[0], b->get_stride(), &one,
                    solve_struct->factor_descr, matrix->get_const_values(),
                    matrix->get_const_row_ptrs(), matrix->get_const_col_idxs(),
                    solve_struct->solve_info, temp_b, b->get_size()[0],
                    x->get_values(), x->get_size()[0]);
            } else {
                dense::transpose(exec, trans_b, b);
                dense::transpose(exec, trans_x, x);
                hipsparse::csrsm_solve(
                    handle, HIPSPARSE_OPERATION_NON_TRANSPOSE,
                    matrix->get_size()[0], trans_b->get_size()[0], &one,
                    solve_struct->factor_descr, matrix->get_const_values(),
                    matrix->get_const_row_ptrs(), matrix->get_const_col_idxs(),
                    solve_struct->solve_info, trans_b->get_values(),
                    trans_b->get_size()[1], trans_x->get_values(),
                    trans_x->get_size()[1]);
                dense::transpose(exec, x, trans_x);
            }
        }


#endif


    } else {
        GKO_NOT_IMPLEMENTED;
    }
}


}  // namespace
}  // namespace hip
}  // namespace kernels
}  // namespace gko


#endif