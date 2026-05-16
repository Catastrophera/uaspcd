#include <stdio.h>
#include <stdlib.h>
#include "image_io.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

float* readImage(const char* filepath, int* width, int* height) {
    int channels;
    // Load as 4 channels (RGBA)
    unsigned char* img_data = stbi_load(filepath, width, height, &channels, 4);
    if (!img_data) {
        fprintf(stderr, "Error: Failed to load image %s\n", filepath);
        return NULL;
    }

    int total_pixels = (*width) * (*height) * 4;
    float* float_data = (float*)malloc(total_pixels * sizeof(float));
    if (!float_data) {
        fprintf(stderr, "Error: Memory allocation failed for image float data\n");
        stbi_image_free(img_data);
        return NULL;
    }

    // Convert unsigned char [0, 255] to float [0.0, 1.0]
    for (int i = 0; i < total_pixels; i++) {
        float_data[i] = img_data[i] / 255.0f;
    }

    stbi_image_free(img_data);
    return float_data;
}

int writeImage(const char* filepath, const float* data, int width, int height) {
    int total_pixels = width * height * 4;
    unsigned char* out_data = (unsigned char*)malloc(total_pixels * sizeof(unsigned char));
    if (!out_data) {
        fprintf(stderr, "Error: Memory allocation failed for output image data\n");
        return 0;
    }

    for (int i = 0; i < total_pixels; i++) {
        float val = data[i] * 255.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;
        out_data[i] = (unsigned char)val;
    }

    int success = stbi_write_png(filepath, width, height, 4, out_data, width * 4);
    
    free(out_data);
    return success;
}
