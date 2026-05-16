#ifndef ENTROPY_H
#define ENTROPY_H

// Menghitung Information Entropy dari gambar float [0.0, 1.0]
// Berguna untuk memvalidasi apakah UM telah meningkatkan detail gambar (entropy naik)
float calculateEntropy(const float* image, int width, int height);

#endif // ENTROPY_H
