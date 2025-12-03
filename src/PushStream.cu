#include "PushStream.cuh"
#include <cuda_runtime.h>

// BGR到YUV420P的CUDA实现 - 优化版本
__global__ void bgr2yuv420p_kernel(const uint8_t *bgr, uint8_t *yuv_y, uint8_t *yuv_u, uint8_t *yuv_v,
                                   int width, int height, int bgr_step)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        int bgr_idx = y * bgr_step + x * 3;

        float b = bgr[bgr_idx];
        float g = bgr[bgr_idx + 1];
        float r = bgr[bgr_idx + 2];

        // ITU-R BT.601转换公式
        float Y = 0.299f * r + 0.587f * g + 0.114f * b;
        float U = -0.14713f * r - 0.28886f * g + 0.436f * b + 128.0f;
        float V = 0.615f * r - 0.51499f * g - 0.10001f * b + 128.0f;

        Y = fmaxf(0.0f, fminf(255.0f, Y));
        U = fmaxf(0.0f, fminf(255.0f, U));
        V = fmaxf(0.0f, fminf(255.0f, V));

        int y_idx = y * width + x;
        yuv_y[y_idx] = static_cast<uint8_t>(Y);

        if (x % 2 == 0 && y % 2 == 0)
        {
            int uv_idx = (y / 2) * (width / 2) + (x / 2);
            yuv_u[uv_idx] = static_cast<uint8_t>(U);
            yuv_v[uv_idx] = static_cast<uint8_t>(V);
        }
    }
}

void cudaBGR2YUV420P(const cv::Mat &bgr, uint8_t *yuv, int width, int height)
{
    if (bgr.empty() || bgr.type() != CV_8UC3 || !yuv)
        return;

    cv::Mat continuous_bgr = bgr.isContinuous() ? bgr : bgr.clone();

    uint8_t *d_bgr, *d_yuv_y, *d_yuv_u, *d_yuv_v;

    size_t bgrSize = continuous_bgr.total() * continuous_bgr.elemSize();
    size_t ySize = width * height;
    size_t uvSize = width * height / 4;
    int bgr_step = continuous_bgr.step[0];

    // 分配GPU内存
    if (cudaMalloc(&d_bgr, bgrSize) != cudaSuccess ||
        cudaMalloc(&d_yuv_y, ySize) != cudaSuccess ||
        cudaMalloc(&d_yuv_u, uvSize) != cudaSuccess ||
        cudaMalloc(&d_yuv_v, uvSize) != cudaSuccess)
    {
        cudaFree(d_bgr);
        cudaFree(d_yuv_y);
        cudaFree(d_yuv_u);
        cudaFree(d_yuv_v);
        return;
    }

    // 复制数据到GPU
    if (cudaMemcpy(d_bgr, continuous_bgr.data, bgrSize, cudaMemcpyHostToDevice) != cudaSuccess)
    {
        cudaFree(d_bgr);
        cudaFree(d_yuv_y);
        cudaFree(d_yuv_u);
        cudaFree(d_yuv_v);
        return;
    }

    // 启动CUDA核函数
    dim3 block(16, 16);
    dim3 grid((width + 15) / 16, (height + 15) / 16);
    bgr2yuv420p_kernel<<<grid, block>>>(d_bgr, d_yuv_y, d_yuv_u, d_yuv_v, width, height, bgr_step);

    cudaDeviceSynchronize();

    // 复制结果回CPU
    cudaMemcpy(yuv, d_yuv_y, ySize, cudaMemcpyDeviceToHost);
    cudaMemcpy(yuv + ySize, d_yuv_u, uvSize, cudaMemcpyDeviceToHost);
    cudaMemcpy(yuv + ySize + uvSize, d_yuv_v, uvSize, cudaMemcpyDeviceToHost);

    // 释放GPU内存
    cudaFree(d_bgr);
    cudaFree(d_yuv_y);
    cudaFree(d_yuv_u);
    cudaFree(d_yuv_v);
}