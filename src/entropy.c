#include "entropy.h"
#include <math.h>
#include <stdlib.h>

float calculateEntropy(const float* image, int width, int height) {
    int histogram[256] = {0};
    int total_pixels = width * height;

    // Build histogram from grayscale brightness
    for (int i = 0; i < total_pixels; i++) {
        float r = image[i * 4];
        float g = image[i * 4 + 1];
        float b = image[i * 4 + 2];
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;
        
        float val = gray * 255.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;
        int idx = (int)val;
        histogram[idx]++;
    }

    float entropy = 0.0f;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            float p = (float)histogram[i] / total_pixels;
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}
