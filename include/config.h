#ifndef CONFIG_H
#define CONFIG_H

// Konfigurasi Work-Group OpenCL
#define WG_SIZE_X 16
#define WG_SIZE_Y 16

// Konfigurasi Unsharp Masking
#define UM_LAMBDA 2.5f  // Enhancement coefficient (0.5 - 1.5 default, dibesarkan ke 2.5 agar sangat terlihat efeknya)
#define GAUSS_N   5     // Ukuran template Gaussian diperbesar jadi 5x5
#define GAUSS_SIGMA 1.5f // Standar deviasi Gaussian

// OpenCL Profiling
#define ENABLE_PROFILING 1

#endif // CONFIG_H
