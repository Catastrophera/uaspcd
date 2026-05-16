#ifndef GAUSS_TEMPLATE_H
#define GAUSS_TEMPLATE_H

// Membangun kernel Gaussian 2D
// n: ukuran (harus ganjil, misal 3)
// sigma: standar deviasi
// return: array float berukuran n*n yang dialokasikan (caller wajib free)
float* buildGaussKernel(int n, float sigma);

#endif // GAUSS_TEMPLATE_H
