#ifndef MEM_TRANSFER_H
#define MEM_TRANSFER_H

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

// Buat buffer di device
cl_mem allocDeviceBuffer(cl_context context, size_t size, cl_mem_flags flags);

// Upload data dari host ke device buffer via Map
int uploadToDevice(cl_command_queue queue, cl_mem buffer, size_t size, const void* host_ptr);

// Download data dari device buffer ke host via Read
int downloadFromDevice(cl_command_queue queue, cl_mem buffer, size_t size, void* host_ptr);

#endif // MEM_TRANSFER_H
