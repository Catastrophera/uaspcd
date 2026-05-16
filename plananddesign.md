# OCL\_UM — Design Document
> Implementasi Parallel Unsharp Masking Image Enhancement berbasis OpenCL  
> Referensi: Song et al., *Scientific Reports* (2022) 12:20175

---

## Daftar Isi

- [Ringkasan Proyek](#ringkasan-proyek)
- [Arsitektur Sistem](#arsitektur-sistem)
- [Struktur File & Modul](#struktur-file--modul)
- [Alur Data Pipeline](#alur-data-pipeline)
- [Spesifikasi Input & Pre-processing](#spesifikasi-input--pre-processing)
- [Memori GPU — Strategi Alokasi](#memori-gpu--strategi-alokasi)
- [Desain Kernel OpenCL](#desain-kernel-opencl)
- [Konfigurasi Work-Group](#konfigurasi-work-group)
- [Formula Matematika](#formula-matematika)
- [Profiling Baseline (CPU Serial)](#profiling-baseline-cpu-serial)
- [Target Performa](#target-performa)
- [Checklist Implementasi](#checklist-implementasi)

---

## Ringkasan Proyek

```
Platform   : CPU + GPU (heterogeneous, OpenCL)
Algoritma  : Unsharp Masking (UM) dengan Gaussian low-pass filter
Target     : Speedup 16.71× vs CPU serial
Input      : Matriks piksel float, resolusi 525×525 s/d 16364×8182
Output     : Gambar ter-enhance dengan edge/detail diperkuat
```

### Kontribusi Utama Jurnal (yang diimplementasi)

1. UM processing diparalelkan di GPU via OpenCL — setiap work-item mengerjakan satu piksel
2. Optimasi memori: Gaussian template → **constant memory**, sub-image tile → **local memory**
3. Multi-point access technique untuk mengurangi redundant read dari global memory
4. Cross-platform: performa setara di AMD RX 5700 XT dan NVIDIA GTX 1060

---

## Arsitektur Sistem

```
┌─────────────────────────────────────────────────────────┐
│                    HOST SIDE (CPU)                      │
│                                                         │
│  [Load Image] → [Read Header] → [Expand Boundary]      │
│      ↓                                                  │
│  [Build Gaussian Template] → [Init OpenCL Context]      │
│      ↓                                                  │
│  [clCreateBuffer] → [clEnqueueMapBuffer] → [Upload]     │
│      ↓                         ↑                        │
│  [Set Kernel Args] → [Launch NDRange]  [Download Result]│
│                                ↓                        │
└────────────────────────────────┼────────────────────────┘
                                 │
┌────────────────────────────────┼────────────────────────┐
│               DEVICE SIDE (GPU)│                        │
│                                ↓                        │
│  ┌─────────────────────────────────────────────────┐    │
│  │              NDRange (Global Work)              │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │    │
│  │  │ Work-group  │  │ Work-group  │  │   ...   │ │    │
│  │  │  (16×16)    │  │  (16×16)    │  │         │ │    │
│  │  │ ┌──┬──┬──┐  │  │             │  │         │ │    │
│  │  │ │wi│wi│wi│  │  │             │  │         │ │    │
│  │  │ ├──┼──┼──┤  │  │             │  │         │ │    │
│  │  │ │wi│wi│wi│  │  │             │  │         │ │    │
│  │  │ └──┴──┴──┘  │  │             │  │         │ │    │
│  │  │ local mem   │  │  local mem  │  │         │ │    │
│  │  └─────────────┘  └─────────────┘  └─────────┘ │    │
│  └────────────────────────────────────────────────-┘    │
│                         ↕                               │
│              Global Memory (VRAM)                       │
│    [srcImageDataEx]          [desImageData]             │
│                 ↕                                       │
│           Constant Memory                               │
│          [GaussTemplate]                                │
└─────────────────────────────────────────────────────────┘
```

---

## Struktur File & Modul

```
project/
│
├── src/
│   ├── main.c                  # Entry point, orkestrasi pipeline, timer
│   ├── image_io.c/.h           # Baca/tulis file gambar (float pixel matrix)
│   ├── boundary.c/.h           # Ekspansi batas: H → H + n - 1
│   ├── gauss_template.c/.h     # Build & normalisasi kernel Gaussian n×n
│   ├── ocl_context.c/.h        # Init OpenCL: platform, device, ctx, queue
│   ├── mem_transfer.c/.h       # clCreateBuffer, Map, Upload, Download
│   └── entropy.c/.h            # Hitung information entropy (validasi output)
│
├── kernels/
│   └── um_kernel.cl            # Kernel GPU: Gaussian conv + UM processing
│
├── include/
│   └── config.h                # Konstanta global (WG size, lambda, n, sigma)
│
├── test/
│   ├── images/                 # Dataset pengujian (7 resolusi)
│   └── validate.c              # Bandingkan entropy sebelum/sesudah
│
└── design.md                   # ← file ini
```

### Deskripsi Tiap Modul

#### `image_io.c`
Baca file gambar ke matriks piksel 1D bertipe `float`. Satu piksel = 4 Byte.  
Fungsi utama: `readImage()`, `writeImage()`, `readImageHeader()`

#### `boundary.c`
Ekspansi gambar dari ukuran H×H menjadi (H+n−1)×(H+n−1).  
Piksel padding diisi dengan nilai border (replicate padding).  
Fungsi utama: `expandImage(src, dst, H, n)`, `getExpandedSize(H, n)`

#### `gauss_template.c`
Komputasi kernel Gaussian 2D dengan standar deviasi σ, lalu normalisasi sehingga total bobot = 1.  
Fungsi utama: `buildGaussKernel(n, sigma)`, `normalizeKernel(kernel, n)`

#### `ocl_context.c`
Inisialisasi lengkap OpenCL runtime.  
Fungsi utama: `initOpenCL()`, `compileKernelFromFile(path)`, `releaseAll()`

#### `mem_transfer.c`
Abstraksi transfer data Host ↔ Device.  
Fungsi utama: `allocDeviceBuffer(size, flags)`, `uploadToDevice()`, `downloadFromDevice()`

#### `um_kernel.cl`
Kernel GPU. Setiap work-item bertanggung jawab atas satu piksel output.  
Menggunakan local memory tiling untuk efisiensi bandwidth.

---

## Alur Data Pipeline

```
[File Gambar]
      │
      ▼
 readImage()              ← CPU, ~768 ms (13.45% total)
      │
      ▼
 expandImage()            ← CPU, ~974 ms (17.06% total)
 H×H → (H+n−1)×(H+n−1)
      │
      ▼
 buildGaussKernel()       ← CPU, ~0.5 ms (0.03% total)
      │
      ▼
 allocDeviceBuffer()      ← OpenCL API
 uploadToDevice()
      │
      ▼
 clEnqueueNDRangeKernel() ← GPU KERNEL ~168 ms (terakselerasi!)
 [Gaussian Filter]
 [UM Processing]          ← hotspot: 47.39% waktu serial
      │
      ▼
 downloadFromDevice()     ← GPU → CPU
      │
      ▼
 writeImage()             ← CPU, ~1260 ms (22.07% total)
      │
      ▼
 [File Output]
```

> **Catatan Bottleneck:** I/O (read + write) menyumbang ~35% total waktu dan **tidak** terakselerasi GPU. Skalanya linear terhadap ukuran gambar.

---

## Spesifikasi Input & Pre-processing

### Format Data
| Parameter       | Nilai                                     |
|----------------|-------------------------------------------|
| Tipe piksel    | `float` (32-bit floating point)           |
| Alokasi/piksel | 4 Byte                                    |
| Layout         | Matriks 1D atau 2D (row-major)            |
| Resolusi uji   | 525×525 hingga 16364×8182                 |

### Boundary Expansion

Untuk gambar berukuran H×H dengan template konvolusi n×n:

```
H_new = H + n - 1
```

**Contoh:** H=7682, n=3 → H_new = 7684

**Alasan:** Menghilangkan kebutuhan cek `if-else` di tepi gambar pada kernel GPU. Semua work-item mengikuti *satu* path komputasi yang seragam → GPU lebih efisien.

### Transfer Memori Host → Device

```
Step 1: clCreateBuffer(CL_MEM_READ_ONLY,  size_input)   → buf_src
Step 2: clCreateBuffer(CL_MEM_WRITE_ONLY, size_output)  → buf_dst
Step 3: clCreateBuffer(CL_MEM_READ_ONLY,  size_template)→ buf_gauss (constant)
Step 4: clEnqueueMapBuffer(buf_src) → isi dengan srcImageDataEx
Step 5: clSetKernelArg() untuk tiap buffer
```

---

## Memori GPU — Strategi Alokasi

```
                    Kecepatan Akses
 Lambat ◄──────────────────────────────────► Cepat

 Global     Constant    L2 Cache   Local      Register
 Memory     Memory                 Memory
 400-600cy  cached      medium     1-16cy     ~1cy
 ████████   ██████       ████       ███        █
 Kapasitas
 Besar ◄──────────────────────────────────► Kecil
```

### Mapping Data ke Tipe Memori

| Data                    | Tipe Memori     | Alasan                                          |
|------------------------|-----------------|------------------------------------------------|
| `srcImageDataEx`       | Global Memory   | Data besar, diakses oleh semua work-items       |
| `desImageData`         | Global Memory   | Output, perlu ditulis dari semua work-items     |
| `GaussTemplate[n×n]`  | Constant Memory | Konstanta, read-only, broadcast ke semua WI     |
| Sub-image tile         | Local Memory    | Shared per work-group, akses cepat              |
| `tidx`, `tidy`, akum. | Register        | Per work-item, paling cepat                     |

### Keuntungan Constant Memory untuk Gaussian Template

- Read-only di seluruh eksekusi kernel → aman dimasukkan constant memory
- Dibroadcast ke semua work-items secara efisien via L2 cache
- Kapasitas template kecil (misal: 3×3 = 9 float = 36 byte) — jauh di bawah limit 64KB

---

## Desain Kernel OpenCL

### Pseudocode Kernel Utama

```c
__kernel void um_kernel(
    __global  float* srcEx,        // input (expanded)
    __global  float* dst,          // output
    __constant float* gaussTpl,    // Gaussian template
    int H, int H_new, int n, float lambda
) {
    // 1. Hitung koordinat global work-item
    int tidx = get_local_id(0) + get_group_id(0) * get_local_size(0);
    int tidy = get_local_id(1) + get_group_id(1) * get_local_size(1);

    // 2. Deklarasi local memory tile
    __local float SubImage[TILE_H + MAX_N][TILE_W + MAX_N];

    // 3. Load data dari global ke local memory
    // (setiap WI load satu atau lebih piksel ke SubImage)
    SubImage[local_y][local_x] = srcEx[global_idx];

    // 4. Sinkronisasi — tunggu semua WI selesai load
    barrier(CLK_LOCAL_MEM_FENCE);

    // 5. Konvolusi Gaussian (low-pass filter)
    float lp = 0.0f;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            lp += SubImage[local_y+i][local_x+j] * gaussTpl[i*n+j];

    // 6. UM Enhancement Formula
    float pixel = srcEx[global_idx_center];
    float hp    = pixel - lp;           // high-frequency component
    float fout  = pixel + lambda * hp;  // enhanced output

    // 7. Clamp & write output
    dst[tidx + tidy * H] = clamp(fout, 0.0f, 1.0f);
}
```

### Multi-Point Access Optimization

Satu work-item memproses BX×BY piksel sekaligus (default BX=BY=2).  
Data 36 piksel yang dibutuhkan untuk 16 piksel output di-load sekali ke local memory, lalu dipakai berulang — mengurangi akses global memory sebesar ~4×.

```
t(i,j) memproses:
  p(i,j)   p(i+1,j)
  p(i,j+1) p(i+1,j+1)
```

---

## Konfigurasi Work-Group

### Benchmark Dimensi Work-Group (image 7682×8182, template 3×3)

| Work-group | Total WI | Waktu (ms) | Keterangan         |
|-----------|----------|------------|--------------------|
| 8×8       | 64       | 170.89     |                    |
| **16×16** | **256**  | **167.94** | **← Optimal ✓**   |
| 24×24     | 576      | 178.09     |                    |
| 32×32     | 1024     | 174.58     |                    |

**Kesimpulan:** Gunakan `WG_SIZE = 16×16` di `config.h`.

### Aturan Pemilihan Work-Group Size

- Harus kelipatan dari **warp size = 32**
- Tidak boleh melebihi **1024 work-items** per group (hardware limit)
- 16×16 = 256 — sweet spot occupancy untuk kebanyakan GPU modern

### NDRange Launch (dari `main.c`)

```c
size_t globalSize[2] = { H, H };         // total work-items
size_t localSize[2]  = { 16, 16 };       // work-group size

clEnqueueNDRangeKernel(
    queue, kernel, 2, NULL,
    globalSize, localSize,
    0, NULL, &event
);
```

---

## Formula Matematika

### Unsharp Masking (Persamaan Utama)

```
f_out(x,y) = f(x,y) + λ × [ f(x,y) − LP_f(x,y) ]
           = f(x,y) + λ × HP_f(x,y)
```

Di mana:
- `f(x,y)`     = piksel input asli
- `LP_f(x,y)` = hasil Gaussian low-pass filter
- `HP_f(x,y)` = komponen high-frequency (tepi & detail)
- `λ`          = enhancement coefficient (rekomenasi: 0.5–1.5)

### Gaussian Filter 2D

```
G(x,y) = 1/(2πσ²) × exp(−(x²+y²) / 2σ²)    untuk −k ≤ x,y ≤ k
```

### Kompleksitas Waktu

| Algoritma | Kompleksitas          | Keterangan                          |
|----------|----------------------|-------------------------------------|
| Serial   | O(H²n²)              | Dominasi UM processing              |
| Paralel  | O(n²)                | Semua piksel diproses simultan      |
| Paralel* | O(H²n² / tsum)       | Jika tidak semua piksel satu kernel |

> `tsum` = jumlah work-items aktif di GPU (selalu besar → O(n²) praktis)

---

## Profiling Baseline (CPU Serial)

Data dari jurnal, image 7682×8182, template 3×3, average 20 run:

```
┌─────────────────────────────────┬──────────┬─────────┐
│ Step                            │  ms      │    %    │
├─────────────────────────────────┼──────────┼─────────┤
│ Read in image data              │  768.30  │  13.45% │
│ Extended image (padding)        │  974.42  │  17.06% │
│ Gaussian template calculation   │    0.53  │   0.03% │
│ Unsharp masking processing  ◄── │ 2705.50  │  47.39% │  ← TARGET GPU
│ Output image enhancement        │ 1260.55  │  22.07% │
├─────────────────────────────────┼──────────┼─────────┤
│ TOTAL                           │ 5709.30  │ 100.00% │
└─────────────────────────────────┴──────────┴─────────┘
```

---

## Target Performa

### Speedup yang Harus Dicapai

| Algoritma       | Platform        | Target Speedup vs CPU |
|----------------|-----------------|----------------------|
| OMP_UM         | CPU multi-core  | ~4.54×               |
| CUDA_UM        | NVIDIA GPU      | ~16.70×              |
| **OCL_UM**     | **AMD / NVIDIA**| **~16.71× ← kita**  |

### Speedup per Resolusi (OCL_UM NVIDIA, dari jurnal)

```
525×525    →   8.45×   ████
750×750    →  14.13×   ██████████
978×1024   →  14.69×   ██████████
1893×2048  →  14.94×   ██████████
3877×4096  →  15.16×   ████████████
7682×8182  →  16.11×   █████████████
16364×8182 →  16.71×   █████████████ ← MAX
```

### Validasi Kualitas Output

Entropy informasi gambar harus **naik** setelah UM enhancement (lebih banyak detail):

| Gambar       | Sebelum UM | Sesudah UM |
|-------------|-----------|-----------|
| Word        | 2.74      | 3.02      |
| Autumn scene| 6.50      | 6.87      |
| White goose | 6.06      | 6.39      |
| Cameraman   | 6.85      | 7.02      |

> CPU_UM, OMP_UM, CUDA_UM, OCL_UM **harus menghasilkan entropy yang sama** — ini bukti kebenaran implementasi paralel.

---

## Checklist Implementasi

### Phase 1 — Setup & CPU Side

- [ ] Inisialisasi proyek, struktur direktori sesuai layout di atas
- [ ] Implementasi `image_io.c` — baca gambar ke `float*` array
- [ ] Implementasi `boundary.c` — ekspansi `H → H+n-1`, padding replicate
- [ ] Implementasi `gauss_template.c` — build + normalize Gaussian n×n
- [ ] Verifikasi output CPU pipeline sebelum masuk ke OpenCL
- [ ] Implementasi timer profiling per-step (untuk benchmark)

### Phase 2 — OpenCL Infrastructure

- [ ] Implementasi `ocl_context.c` — query platform, device, buat context & queue
- [ ] Implementasi `mem_transfer.c` — `clCreateBuffer`, map, upload, download
- [ ] Load & kompilasi `um_kernel.cl` via `clCreateProgramWithSource`
- [ ] Error handling untuk semua OpenCL API call (cek return code)

### Phase 3 — GPU Kernel

- [ ] Tulis `um_kernel.cl` — koordinat tidx/tidy, local memory tile
- [ ] Implementasi load data ke `__local SubImage[][]`
- [ ] Tambah `barrier(CLK_LOCAL_MEM_FENCE)` setelah load
- [ ] Loop konvolusi Gaussian menggunakan `__constant gaussTpl`
- [ ] Implementasi formula UM: `fout = f + lambda * (f - lp)`
- [ ] Clamp output ke range valid `[0.0f, 1.0f]`
- [ ] Tulis hasil ke `__global dst[]`

### Phase 4 — Optimization

- [ ] Verifikasi work-group size 16×16 (set di `config.h`)
- [ ] Benchmark 4 variasi work-group (8×8, 16×16, 24×24, 32×32)
- [ ] Implementasi multi-point access (BX×BY per work-item)
- [ ] Pastikan Gaussian template di `__constant` (bukan `__global`)

### Phase 5 — Validasi & Benchmark

- [ ] Bandingkan output OCL_UM vs CPU_UM secara visual
- [ ] Hitung entropy sebelum/sesudah — harus cocok dengan tabel jurnal
- [ ] Benchmark 7 resolusi: 525×525 s/d 16364×8182
- [ ] Hitung speedup: `SOCL = T_cpu / T_ocl` untuk setiap resolusi
- [ ] Dokumentasi hasil dalam tabel (sesuai format Table 4 & 5 jurnal)

---

## Catatan Penting

> **Lambda (λ):** Nilai default = 1.0. Range aman: 0.5–1.5. Di luar range ini, hasil bisa over-sharpened atau tidak ada perubahan signifikan.

> **Sigma (σ):** Untuk template 3×3, gunakan σ = 1.0 (standar jurnal).

> **Tipe Data:** Seluruh komputasi menggunakan `float` (single precision) — sesuai jurnal. Jangan mix dengan `double` di kernel.

> **Index Boundary:** Pastikan index akses `SubImage` tidak keluar batas tile saat work-item di tepi work-group. Ukuran tile harus `(WG_SIZE + n - 1) × (WG_SIZE + n - 1)`.

> **Platform Test:** Uji di minimal 1 GPU (NVIDIA atau AMD). Jika keduanya tersedia, bandingkan speedup — seharusnya hasilnya mendekati identik (RSCUDA-OCL ≈ 1.00–1.11).

---

*Referensi: Yupu Song, Cailin Li, et al. "Unsharp masking image enhancement the parallel algorithm based on cross-platform." Scientific Reports 12, 20175 (2022). DOI: 10.1038/s41598-022-21745-9*
