#include "boundary.h"

int getExpandedSize(int originalSize, int n) {
    return originalSize + n - 1;
}

void expandImage(const float* src, float* dst, int W, int H, int n) {
    int W_new = getExpandedSize(W, n);
    int H_new = getExpandedSize(H, n);
    int pad = n / 2; // Untuk n=3, pad=1

    for (int y = 0; y < H_new; y++) {
        for (int x = 0; x < W_new; x++) {
            // Mapping koordinat baru ke koordinat lama dengan clamping (replicate padding)
            int src_x = x - pad;
            int src_y = y - pad;

            if (src_x < 0) src_x = 0;
            if (src_x >= W) src_x = W - 1;

            if (src_y < 0) src_y = 0;
            if (src_y >= H) src_y = H - 1;

            int dst_idx = (y * W_new + x) * 4;
            int src_idx = (src_y * W + src_x) * 4;
            
            dst[dst_idx] = src[src_idx];
            dst[dst_idx + 1] = src[src_idx + 1];
            dst[dst_idx + 2] = src[src_idx + 2];
            dst[dst_idx + 3] = src[src_idx + 3];
        }
    }
}
