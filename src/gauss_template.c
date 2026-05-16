#include <stdlib.h>
#include <math.h>
#include "gauss_template.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float* buildGaussKernel(int n, float sigma) {
    float* kernel = (float*)malloc(n * n * sizeof(float));
    if (!kernel) return NULL;

    int half = n / 2;
    float sum = 0.0f;

    for (int y = -half; y <= half; y++) {
        for (int x = -half; x <= half; x++) {
            float val = expf(-(x * x + y * y) / (2.0f * sigma * sigma)) / (2.0f * M_PI * sigma * sigma);
            kernel[(y + half) * n + (x + half)] = val;
            sum += val;
        }
    }

    // Normalisasi kernel agar total bobot = 1.0
    for (int i = 0; i < n * n; i++) {
        kernel[i] /= sum;
    }

    return kernel;
}
