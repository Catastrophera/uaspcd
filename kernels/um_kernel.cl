#define WG_SIZE_X 16
#define WG_SIZE_Y 16
#define BX 2
#define BY 2
#define MAX_N 15 // batas max template size jika diperlukan
#define TILE_W WG_SIZE_X
#define TILE_H WG_SIZE_Y

__kernel void um_kernel(
    __global const float4* srcEx,
    __global float4* dst,
    __constant float* gaussTpl,
    int W, int H, int W_new, int H_new, int n, float lambda
) {
    int tidx = get_global_id(0);
    int tidy = get_global_id(1);

    int local_x = get_local_id(0);
    int local_y = get_local_id(1);

    int pad = n / 2;

    // Multi-Point Access: 1 Work-Group (16x16) memproses area output (32x32).
    // Local memory menampung area output + padding
    __local float4 SubImage[TILE_H * BY + MAX_N - 1][TILE_W * BX + MAX_N - 1];

    int local_w = TILE_W * BX + n - 1;
    int local_h = TILE_H * BY + n - 1;

    // Koordinat global tile mulai dari pojok kiri atas work-group di input expanded
    int group_x = get_group_id(0) * TILE_W * BX;
    int group_y = get_group_id(1) * TILE_H * BY;

    // Load data ke SubImage secara kooperatif
    int total_local = local_w * local_h;
    int total_threads = TILE_W * TILE_H;
    int thread_id = local_y * TILE_W + local_x;

    for (int i = thread_id; i < total_local; i += total_threads) {
        int ly = i / local_w;
        int lx = i % local_w;
        
        int gx = group_x + lx;
        int gy = group_y + ly;
        
        // Clamp ke batas W_new, H_new
        if (gx >= W_new) gx = W_new - 1;
        if (gy >= H_new) gy = H_new - 1;

        SubImage[ly][lx] = srcEx[gy * W_new + gx];
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // Hitung output untuk block BX x BY piksel yang menjadi tanggung jawab work-item ini
    int base_x = tidx * BX;
    int base_y = tidy * BY;

    for (int dy = 0; dy < BY; dy++) {
        for (int dx = 0; dx < BX; dx++) {
            int out_x = base_x + dx;
            int out_y = base_y + dy;

            if (out_x < W && out_y < H) {
                float4 lp = (float4)(0.0f);
                int sub_x = local_x * BX + dx;
                int sub_y = local_y * BY + dy;

                for (int i = 0; i < n; i++) {
                    for (int j = 0; j < n; j++) {
                        lp += SubImage[sub_y + i][sub_x + j] * gaussTpl[i * n + j];
                    }
                }

                // Piksel tengah (center) dari template
                float4 pixel = SubImage[sub_y + pad][sub_x + pad];
                
                // UM Formula
                float4 hp = pixel - lp;
                float4 fout = pixel + lambda * hp;

                // Preserve original alpha
                fout.w = pixel.w;

                // Clamp
                fout = clamp(fout, (float4)(0.0f), (float4)(1.0f));

                dst[out_y * W + out_x] = fout;
            }
        }
    }
}
