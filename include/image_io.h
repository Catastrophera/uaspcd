#ifndef IMAGE_IO_H
#define IMAGE_IO_H

// Baca gambar (akan dikonversi ke grayscale float array [0.0, 1.0])
// Mengembalikan pointer ke array 1D float. Caller wajib free().
// width dan height akan diisi dengan resolusi gambar.
float* readImage(const char* filepath, int* width, int* height);

// Tulis gambar (dari grayscale float array [0.0, 1.0] ke gambar 8-bit)
// Mengembalikan 1 jika sukses, 0 jika gagal.
int writeImage(const char* filepath, const float* data, int width, int height);

#endif // IMAGE_IO_H
