#ifndef OCL_CONTEXT_H
#define OCL_CONTEXT_H

#include <CL/cl.h>

// Struktur konteks OpenCL untuk mempermudah passing data
typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
} OCLContext;

// Inisialisasi OpenCL, membuat konteks, antrian, mengkompilasi kernel
int initOpenCL(OCLContext* ocl, const char* kernel_path);

// Cleanup sumber daya OpenCL
void releaseOpenCL(OCLContext* ocl);

#endif // OCL_CONTEXT_H
