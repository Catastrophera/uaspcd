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
    cl_uint num_platforms = 0;

    err = clGetPlatformIDs(0, NULL, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        fprintf(stderr, "Failed to find any OpenCL platforms\n");
        return 0;
    }

    cl_platform_id* platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * num_platforms);
    if (!platforms) {
        fprintf(stderr, "Out of memory allocating platforms array\n");
        return 0;
    }

    err = clGetPlatformIDs(num_platforms, platforms, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to retrieve OpenCL platform IDs\n");
        free(platforms);
        return 0;
    }

    int found_device = 0;

    // 1. Try to find a GPU on any platform
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = 0;
        err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 1, &ocl->device, &num_devices);
        if (err == CL_SUCCESS && num_devices > 0) {
            ocl->platform = platforms[i];
            found_device = 1;
            break;
        }
    }

    // 2. Fallback to CPU on any platform if no GPU is found
    if (!found_device) {
        for (cl_uint i = 0; i < num_platforms; i++) {
            cl_uint num_devices = 0;
            err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_CPU, 1, &ocl->device, &num_devices);
            if (err == CL_SUCCESS && num_devices > 0) {
                ocl->platform = platforms[i];
                found_device = 1;
                break;
            }
        }
    }

    free(platforms);

    if (!found_device) {
        fprintf(stderr, "Failed to find any compatible OpenCL GPU or CPU device\n");
        return 0;
    }

    char platform_name[128];
    char device_name[128];
    clGetPlatformInfo(ocl->platform, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);
    clGetDeviceInfo(ocl->device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    printf("Using OpenCL Platform: %s\n", platform_name);
    printf("Using OpenCL Device:   %s\n", device_name);

    cl_ulong local_mem;
    size_t max_wg;
    clGetDeviceInfo(ocl->device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(local_mem), &local_mem, NULL);
    clGetDeviceInfo(ocl->device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_wg), &max_wg, NULL);
    printf("Device Max Local Memory: %llu KB\n", (unsigned long long)local_mem / 1024);
    printf("Device Max Work Group Size: %zu\n", max_wg);

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
