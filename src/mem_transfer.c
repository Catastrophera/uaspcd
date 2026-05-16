#include "mem_transfer.h"
#include <string.h>

cl_mem allocDeviceBuffer(cl_context context, size_t size, cl_mem_flags flags) {
    cl_int err;
    cl_mem buf = clCreateBuffer(context, flags, size, NULL, &err);
    return buf;
}

int uploadToDevice(cl_command_queue queue, cl_mem buffer, size_t size, const void* host_ptr) {
    cl_int err;
    void* mapped_ptr = clEnqueueMapBuffer(queue, buffer, CL_TRUE, CL_MAP_WRITE, 0, size, 0, NULL, NULL, &err);
    if (err != CL_SUCCESS || !mapped_ptr) return 0;
    
    memcpy(mapped_ptr, host_ptr, size);
    
    err = clEnqueueUnmapMemObject(queue, buffer, mapped_ptr, 0, NULL, NULL);
    return (err == CL_SUCCESS);
}

int downloadFromDevice(cl_command_queue queue, cl_mem buffer, size_t size, void* host_ptr) {
    cl_int err = clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, size, host_ptr, 0, NULL, NULL);
    return (err == CL_SUCCESS);
}
