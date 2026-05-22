#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "config.h"
#include "image_io.h"
#include "boundary.h"
#include "gauss_template.h"
#include "ocl_context.h"
#include "mem_transfer.h"
#include "entropy.h"

// Timer helper (Windows specific with QueryPerformanceCounter for high res)
#include <windows.h>
double get_time_ms() {
    LARGE_INTEGER freq, val;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&val);
    return (double)val.QuadPart * 1000.0 / (double)freq.QuadPart;
}

// CPU Fallback / Baseline UM for RGBA
void cpu_um(const float* srcEx, float* dst, const float* gaussTpl, int W, int H, int W_new, int n, float lambda) {
    int pad = n / 2;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float lp[3] = {0.0f, 0.0f, 0.0f};
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    int src_idx = ((y + i) * W_new + (x + j)) * 4;
                    float weight = gaussTpl[i * n + j];
                    lp[0] += srcEx[src_idx] * weight;
                    lp[1] += srcEx[src_idx + 1] * weight;
                    lp[2] += srcEx[src_idx + 2] * weight;
                }
            }
            int center_idx = ((y + pad) * W_new + (x + pad)) * 4;
            int dst_idx = (y * W + x) * 4;
            
            for (int c = 0; c < 3; c++) {
                float pixel = srcEx[center_idx + c];
                float hp = pixel - lp[c];
                float fout = pixel + lambda * hp;
                if (fout < 0.0f) fout = 0.0f;
                if (fout > 1.0f) fout = 1.0f;
                dst[dst_idx + c] = fout;
            }
            // Preserve Alpha
            dst[dst_idx + 3] = srcEx[center_idx + 3];
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <input_image> <output_image>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    int n = argc > 3 ? atoi(argv[3]) : GAUSS_N;
    if (n % 2 == 0) n += 1;
    if (n > 13) n = 13;
    if (n < 3) n = 3;

    float lambda = argc > 4 ? atof(argv[4]) : UM_LAMBDA;

    int W, H;
    double t_start, t_end;

    // 1. Read Image
    t_start = get_time_ms();
    float* srcImage = readImage(input_file, &W, &H);
    t_end = get_time_ms();
    if (!srcImage) return 1;
    printf("Read image: %.2f ms (W:%d, H:%d)\n", t_end - t_start, W, H);

    float entropy_before = calculateEntropy(srcImage, W, H);
    printf("Entropy before UM: %.4f\n", entropy_before);

    // 2. Expand Boundary
    t_start = get_time_ms();
    int W_new = getExpandedSize(W, n);
    int H_new = getExpandedSize(H, n);
    float* srcEx = (float*)malloc(W_new * H_new * 4 * sizeof(float));
    expandImage(srcImage, srcEx, W, H, n);
    t_end = get_time_ms();
    printf("Expand boundary: %.2f ms\n", t_end - t_start);

    // 3. Build Gaussian Template
    t_start = get_time_ms();
    float* gaussTpl = buildGaussKernel(n, GAUSS_SIGMA);
    t_end = get_time_ms();
    printf("Build Gauss kernel: %.2f ms\n", t_end - t_start);

    // Baseline CPU verification
    float* dstCPU = (float*)malloc(W * H * 4 * sizeof(float));
    t_start = get_time_ms();
    cpu_um(srcEx, dstCPU, gaussTpl, W, H, W_new, n, lambda);
    t_end = get_time_ms();
    printf("CPU UM processing: %.2f ms\n", t_end - t_start);

    // 4. OpenCL Setup
    t_start = get_time_ms();
    OCLContext ocl;
    if (!initOpenCL(&ocl, "kernels/um_kernel.cl")) {
        fprintf(stderr, "Failed to initialize OpenCL\n");
        return 1;
    }
    t_end = get_time_ms();
    printf("Init OpenCL: %.2f ms\n", t_end - t_start);

    // 5. Memory Transfer
    t_start = get_time_ms();
    cl_mem bufSrcEx = allocDeviceBuffer(ocl.context, W_new * H_new * 4 * sizeof(float), CL_MEM_READ_ONLY);
    cl_mem bufDst = allocDeviceBuffer(ocl.context, W * H * 4 * sizeof(float), CL_MEM_WRITE_ONLY);
    cl_mem bufGauss = allocDeviceBuffer(ocl.context, n * n * sizeof(float), CL_MEM_READ_ONLY);

    uploadToDevice(ocl.queue, bufSrcEx, W_new * H_new * 4 * sizeof(float), srcEx);
    uploadToDevice(ocl.queue, bufGauss, n * n * sizeof(float), gaussTpl);
    t_end = get_time_ms();
    printf("Upload to Device: %.2f ms\n", t_end - t_start);

    // 6. Launch Kernel
    t_start = get_time_ms();
    clSetKernelArg(ocl.kernel, 0, sizeof(cl_mem), &bufSrcEx);
    clSetKernelArg(ocl.kernel, 1, sizeof(cl_mem), &bufDst);
    clSetKernelArg(ocl.kernel, 2, sizeof(cl_mem), &bufGauss);
    clSetKernelArg(ocl.kernel, 3, sizeof(int), &W);
    clSetKernelArg(ocl.kernel, 4, sizeof(int), &H);
    clSetKernelArg(ocl.kernel, 5, sizeof(int), &W_new);
    clSetKernelArg(ocl.kernel, 6, sizeof(int), &H_new);
    clSetKernelArg(ocl.kernel, 7, sizeof(int), &n);
    clSetKernelArg(ocl.kernel, 8, sizeof(float), &lambda);

    // Multi-Point Access parameters matching the kernel (BX=2, BY=2)
    int BX = 2;
    int BY = 2;
    
    // Pad global size to multiple of work-group size
    size_t globalSize[2] = {
        ((W + BX - 1) / BX + WG_SIZE_X - 1) / WG_SIZE_X * WG_SIZE_X,
        ((H + BY - 1) / BY + WG_SIZE_Y - 1) / WG_SIZE_Y * WG_SIZE_Y
    };
    size_t localSize[2] = { WG_SIZE_X, WG_SIZE_Y };

    cl_event event;
    cl_int err = clEnqueueNDRangeKernel(ocl.queue, ocl.kernel, 2, NULL, globalSize, localSize, 0, NULL, &event);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error: clEnqueueNDRangeKernel failed with code %d\n", err);
    } else {
        err = clWaitForEvents(1, &event);
        if (err != CL_SUCCESS) {
            fprintf(stderr, "Error: clWaitForEvents failed with code %d\n", err);
        }
        clReleaseEvent(event);
    }
    t_end = get_time_ms();
    printf("GPU UM processing (Kernel Launch): %.2f ms\n", t_end - t_start);

    // 7. Download Results
    t_start = get_time_ms();
    float* dstGPU = (float*)malloc(W * H * 4 * sizeof(float));
    if (!downloadFromDevice(ocl.queue, bufDst, W * H * 4 * sizeof(float), dstGPU)) {
        fprintf(stderr, "Error: Failed to download results from device\n");
    }
    t_end = get_time_ms();
    printf("Download from Device: %.2f ms\n", t_end - t_start);

    float entropy_after = calculateEntropy(dstGPU, W, H);
    printf("Entropy after UM (GPU): %.4f\n", entropy_after);

    // 8. Write Output Image
    t_start = get_time_ms();
    writeImage(output_file, dstGPU, W, H);
    t_end = get_time_ms();
    printf("Write image: %.2f ms\n", t_end - t_start);

    // Cleanup
    clReleaseMemObject(bufSrcEx);
    clReleaseMemObject(bufDst);
    clReleaseMemObject(bufGauss);
    releaseOpenCL(&ocl);

    free(srcImage);
    free(srcEx);
    free(gaussTpl);
    free(dstCPU);
    free(dstGPU);

    return 0;
}
