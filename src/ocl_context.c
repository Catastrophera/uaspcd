#include "ocl_context.h"
#include <stdio.h>
#include <stdlib.h>

static char* readKernelSource(const char* filepath, size_t* size) {
    FILE* f = fopen(filepath, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    char* src = (char*)malloc(*size + 1);
    fread(src, 1, *size, f);
    src[*size] = '\0';
    fclose(f);
    return src;
}

int initOpenCL(OCLContext* ocl, const char* kernel_path) {
    cl_int err;
    cl_uint num_platforms;

    err = clGetPlatformIDs(1, &ocl->platform, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        fprintf(stderr, "Failed to find OpenCL platform\n");
        return 0;
    }

    err = clGetDeviceIDs(ocl->platform, CL_DEVICE_TYPE_GPU, 1, &ocl->device, NULL);
    if (err != CL_SUCCESS) {
        // Fallback to CPU if GPU not found
        err = clGetDeviceIDs(ocl->platform, CL_DEVICE_TYPE_CPU, 1, &ocl->device, NULL);
        if (err != CL_SUCCESS) {
            fprintf(stderr, "Failed to find OpenCL device\n");
            return 0;
        }
    }

    char device_name[128];
    clGetDeviceInfo(ocl->device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("Using OpenCL Device: %s\n", device_name);

    ocl->context = clCreateContext(NULL, 1, &ocl->device, NULL, NULL, &err);
    if (err != CL_SUCCESS) return 0;

    ocl->queue = clCreateCommandQueue(ocl->context, ocl->device, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) return 0;

    size_t src_size;
    char* src = readKernelSource(kernel_path, &src_size);
    if (!src) {
        fprintf(stderr, "Failed to read kernel source: %s\n", kernel_path);
        return 0;
    }

    ocl->program = clCreateProgramWithSource(ocl->context, 1, (const char**)&src, &src_size, &err);
    free(src);
    if (err != CL_SUCCESS) return 0;

    err = clBuildProgram(ocl->program, 1, &ocl->device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(ocl->program, ocl->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(ocl->program, ocl->device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "Kernel Build Error:\n%s\n", log);
        free(log);
        return 0;
    }

    ocl->kernel = clCreateKernel(ocl->program, "um_kernel", &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create kernel\n");
        return 0;
    }

    return 1;
}

void releaseOpenCL(OCLContext* ocl) {
    if (ocl->kernel) clReleaseKernel(ocl->kernel);
    if (ocl->program) clReleaseProgram(ocl->program);
    if (ocl->queue) clReleaseCommandQueue(ocl->queue);
    if (ocl->context) clReleaseContext(ocl->context);
}
