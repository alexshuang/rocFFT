/******************************************************************************
* Copyright (c) 2016 - present Advanced Micro Devices, Inc. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/
#include "rocfft.h"
#include <fftw3.h>
#include <hip/hip_runtime_api.h>
#include <hip/hip_vector_types.h>
#include <iostream>
#include <math.h>
#include <vector>

int main()
{
    // For size N <= 4096
    const size_t N = 16;

    // FFTW reference compute
    // ==========================================

    std::vector<float2> cx(N);
    fftwf_complex *     in, *out;
    fftwf_plan          p;

    in  = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * N);
    out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * N);
    p   = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    for(size_t i = 0; i < N; i++)
    {
        cx[i].x = in[i][0] = i + (i % 3) - (i % 7);
        cx[i].y = in[i][1] = 0;
    }

    fftwf_execute(p);

    // rocfft gpu compute
    // ========================================

    rocfft_setup();

    size_t Nbytes = N * sizeof(float2);

    // Create HIP device object.
    float2* x;
    hipMalloc(&x, Nbytes);

    //  Copy data to device
    hipMemcpy(x, &cx[0], Nbytes, hipMemcpyHostToDevice);

    // Create plan
    rocfft_plan plan   = NULL;
    size_t      length = N;
    rocfft_plan_create(&plan,
                       rocfft_placement_inplace,
                       rocfft_transform_type_complex_forward,
                       rocfft_precision_single,
                       1,
                       &length,
                       1,
                       NULL);

    // Check if the plan requires a work buffer
    size_t work_buf_size = 0;
    rocfft_plan_get_work_buffer_size(plan, &work_buf_size);
    void*                 work_buf = nullptr;
    rocfft_execution_info info     = nullptr;
    if(work_buf_size)
    {
        rocfft_execution_info_create(&info);
        hipMalloc(&work_buf, work_buf_size);
        rocfft_execution_info_set_work_buffer(info, work_buf, work_buf_size);
    }

    // Execute plan
    rocfft_execute(plan, (void**)&x, NULL, NULL);

    // Clean up work buffer
    if(work_buf_size)
    {
        hipFree(work_buf);
        rocfft_execution_info_destroy(info);
    }

    // Destroy plan
    rocfft_plan_destroy(plan);

    // Copy result back to host
    std::vector<float2> y(N);
    hipMemcpy(&y[0], x, Nbytes, hipMemcpyDeviceToHost);

    double error      = 0;
    size_t element_id = 0;
    for(size_t i = 0; i < N; i++)
    {
        printf("element %d: input %f, %f; FFTW result %f, %f; rocFFT result %f, %f \n",
               (int)i,
               (float)cx[i].x,
               (float)x[i].y,
               (float)out[i][0],
               (float)out[i][1],
               (float)y[i].x,
               (float)y[i].y);
        double err = fabs(out[i][0] - y[i].x) + fabs(out[i][1] - y[i].y);
        if(err > error)
        {
            error      = err;
            element_id = i;
        }
    }

    printf("max error of FFTW and rocFFT is %e at element %d\n",
           error / (fabs(out[element_id][0]) + fabs(out[element_id][1])),
           (int)element_id);

    fftwf_destroy_plan(p);
    fftwf_free(in);
    fftwf_free(out);

    hipFree(x);

    rocfft_cleanup();

    return 0;
}
