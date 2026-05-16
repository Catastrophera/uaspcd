#ifndef BOUNDARY_H
#define BOUNDARY_H

// Menghitung ukuran baru setelah ekspansi batas
int getExpandedSize(int originalSize, int n);

// Ekspansi gambar WxH menjadi W_new x H_new dengan replicate padding.
// src: array input berukuran W * H
// dst: array output berukuran W_new * H_new (harus sudah dialokasikan caller)
// W: lebar asli
// H: tinggi asli
// n: ukuran template (misal 3)
void expandImage(const float* src, float* dst, int W, int H, int n);

#endif // BOUNDARY_H
