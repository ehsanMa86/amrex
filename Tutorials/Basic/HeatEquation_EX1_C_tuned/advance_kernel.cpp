#include "advance_kernel.H"
#include <cstring>
#include <AMReX_BLFort.H>

#ifdef CUDA
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <AMReX_Device.H>
#define BLOCKSIZE_2D 16
#endif

#define ARRAY_2D(PHI, LO_X, LO_Y, HI_X, HI_Y, I, J) PHI[(J-LO_Y)*(HI_X-LO_X+1)+I-LO_X]
#define CUDA_CHECK(x) std::cerr << (x) << std::endl



#ifdef CUDA
__global__
void compute_flux_doit_gpu(
        const int lox, const int loy, const int hix, const int hiy,
        const __restrict__ amrex::Real* phi,
        const int phi_lox, const int phi_loy, const int phi_hix, const int phi_hiy,
        __restrict__ amrex::Real* fluxx,
        const int fluxx_lox, const int fluxx_loy, const int fluxx_hix, const int fluxx_hiy,
        __restrict__ amrex::Real* fluxy,
        const int fluxy_lox, const int fluxy_loy, const int fluxy_hix, const int fluxy_hiy,
        const amrex::Real dx, const amrex::Real dy) 
{
    // map cuda thread (cudai, cudaj) to cell edge (i,j) it works on 
    int cudai = threadIdx.x + blockDim.x * blockIdx.x;
    int cudaj = threadIdx.y + blockDim.y * blockIdx.y;
    int i = cudai + lox;
    int j = cudaj + loy;

    // compute flux
    // flux in x direction
    if ( i <= (hix+1) && j <= hiy ) {
        ARRAY_2D(fluxx,fluxx_lox,fluxx_loy,fluxx_hix,fluxx_hiy,i,j) = 
            ( ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j) - ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i-1,j) ) / dx;
    }
    // flux in y direction
    if ( i <= hix && j <= (hiy+1) ) {
        ARRAY_2D(fluxy,fluxy_lox,fluxy_loy,fluxy_hix,fluxy_hiy,i,j) = 
            ( ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j) - ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j-1) ) / dy;
    }
}



__global__
void update_phi_doit_gpu(
        const int lox, const int loy, const int hix, const int hiy,
        const __restrict__ amrex::Real* phi_old,
        const int phi_old_lox, const int phi_old_loy, const int phi_old_hix, const int phi_old_hiy,
        __restrict__ amrex::Real* phi_new,
        const int phi_new_lox, const int phi_new_loy, const int phi_new_hix, const int phi_new_hiy,
        const __restrict__ amrex::Real* fx,
        const int fx_lox, const int fx_loy, const int fx_hix, const int fx_hiy,
        const __restrict__ amrex::Real* fy,
        const int fy_lox, const int fy_loy, const int fy_hix, const int fy_hiy,
        const amrex::Real dx, const amrex::Real dy, const amrex::Real dt) 
{
    // map cuda thread (cudai, cudaj) to cell edge (i,j) it works on 
    int cudai = threadIdx.x + blockDim.x * blockIdx.x;
    int cudaj = threadIdx.y + blockDim.y * blockIdx.y;
    int i = cudai + lox;
    int j = cudaj + loy;
    if ( i > hix || j > hiy ) return;
    ARRAY_2D(phi_new,phi_new_lox,phi_new_loy,phi_new_hix,phi_new_hiy,i,j) =
        ARRAY_2D(phi_old,phi_old_lox,phi_old_loy,phi_old_hix,phi_old_hiy,i,j) +
        dt/dx * ( ARRAY_2D(fx,fx_lox,fx_loy,fx_hix,fx_hiy,i+1,j) - ARRAY_2D(fx,fx_lox,fx_loy,fx_hix,fx_hiy,i,j) ) +
        dt/dy * ( ARRAY_2D(fy,fy_lox,fy_loy,fy_hix,fy_hiy,i,j+1) - ARRAY_2D(fy,fy_lox,fy_loy,fy_hix,fy_hiy,i,j) ); 
}



void compute_flux_c(const int& lox, const int& loy, const int& hix, const int& hiy,
                  const amrex::Real* phi,
                  const int& phi_lox, const int& phi_loy, const int& phi_hix, const int& phi_hiy,
                  amrex::Real* fluxx,
                  const int& fx_lox, const int& fx_loy, const int& fx_hix, const int& fx_hiy,
                  amrex::Real* fluxy,
                  const int& fy_lox, const int& fy_loy, const int& fy_hix, const int& fy_hiy,
                  const amrex::Real& dx, const amrex::Real& dy, const int& idx) 
{
#if (BL_SPACEDIM == 2)
    dim3 blockSize(BLOCKSIZE_2D,BLOCKSIZE_2D,1);
    dim3 gridSize( (hix-lox+1 + blockSize.x) / blockSize.x, 
                   (hiy-loy+1 + blockSize.y) / blockSize.y, 
                    1 
                 );
    cudaStream_t pStream;
    get_stream(&idx, &pStream);
    compute_flux_doit_gpu<<<gridSize, blockSize, 0, pStream>>>(
            lox, loy, hix, hiy,
            phi,
            phi_lox, phi_loy, phi_hix, phi_hiy,
            fluxx,
            fx_lox, fx_loy, fx_hix, fx_hiy,
            fluxy,
            fy_lox, fy_loy, fy_hix, fy_hiy,
            dx, dy);
#elif (BL_SPACEDIM == 3)
    // TODO
#endif

}


void update_phi_c(const int& lox, const int& loy, const int& hix, const int& hiy,
                const amrex::Real* phi_old,
                const int& phi_old_lox, const int& phi_old_loy, const int& phi_old_hix, const int& phi_old_hiy,
                amrex::Real* phi_new,
                const int& phi_new_lox, const int& phi_new_loy, const int& phi_new_hix, const int& phi_new_hiy,
                const amrex::Real* fluxx,
                const int& fx_lox, const int& fx_loy, const int& fx_hix, const int& fx_hiy,
                const amrex::Real* fluxy,
                const int& fy_lox, const int& fy_loy, const int& fy_hix, const int& fy_hiy,
                const amrex::Real& dx, const amrex::Real& dy, const amrex::Real& dt, const int& idx) 
{
#if (BL_SPACEDIM == 2)
    dim3 blockSize(BLOCKSIZE_2D,BLOCKSIZE_2D,1);
    dim3 gridSize( (hix-lox+1 + blockSize.x) / blockSize.x, 
                   (hiy-loy+1 + blockSize.y) / blockSize.y, 
                    1 
                 );
    cudaStream_t pStream;
    get_stream(&idx, &pStream);
    update_phi_doit_gpu<<<gridSize, blockSize, 0, pStream>>>(
            lox, loy, hix, hiy,
            phi_old,
            phi_old_lox, phi_old_loy, phi_old_hix, phi_old_hiy,
            phi_new,
            phi_new_lox, phi_new_loy, phi_new_hix, phi_new_hiy,
            fluxx,
            fx_lox, fx_loy, fx_hix, fx_hiy,
            fluxy,
            fy_lox, fy_loy, fy_hix, fy_hiy,
            dx, dy, dt);
#elif (BL_SPACEDIM == 3)
    // TODO
#endif
}
#endif //CUDA




/*
 * CPU version
 */

void update_phi_doit_cpu(
            const int& lox, const int& loy, const int& hix, const int& hiy,
            const amrex::Real* __restrict__ phi_old, const int& phi_old_lox, const int& phi_old_loy, const int& phi_old_hix, const int& phi_old_hiy,
            amrex::Real* __restrict__ phi_new, const int& phi_new_lox, const int& phi_new_loy, const int& phi_new_hix, const int& phi_new_hiy,
            const amrex::Real* __restrict__ fx, const int& fx_lox, const int& fx_loy, const int& fx_hix, const int& fx_hiy,
            const amrex::Real* __restrict__ fy, const int& fy_lox, const int& fy_loy, const int& fy_hix, const int& fy_hiy,
            const amrex::Real& dx, const amrex::Real& dy, const amrex::Real& dt)
{
    for (int j = loy; j <= hiy; ++j ) {
        for (int i = lox; i <= hix; ++i ) {
            ARRAY_2D(phi_new,phi_new_lox,phi_new_loy,phi_new_hix,phi_new_hiy,i,j) =
                ARRAY_2D(phi_old,phi_old_lox,phi_old_loy,phi_old_hix,phi_old_hiy,i,j) +
                dt/dx * ( ARRAY_2D(fx,fx_lox,fx_loy,fx_hix,fx_hiy,i+1,j) - ARRAY_2D(fx,fx_lox,fx_loy,fx_hix,fx_hiy,i,j) ) +
                dt/dy * ( ARRAY_2D(fy,fy_lox,fy_loy,fy_hix,fy_hiy,i,j+1) - ARRAY_2D(fy,fy_lox,fy_loy,fy_hix,fy_hiy,i,j) ); 
        }
    }
}

void compute_flux_doit_cpu(
            const int& lox, const int& loy, const int& hix, const int& hiy,
            const amrex::Real* __restrict__ phi, const int& phi_lox, const int& phi_loy, const int& phi_hix, const int& phi_hiy,
            amrex::Real* __restrict__ flux, const int& flux_lox, const int& flux_loy, const int& flux_hix, const int& flux_hiy,
            const amrex::Real& dx, const amrex::Real& dy, const int& idir)
{
    if (idir == 1) {// flux in x direction
        // double dxflip = 1 / dx;
        for (int j = loy; j <= hiy; ++j ) {
            for (int i = lox; i <= hix+1; ++i ) {
                ARRAY_2D(flux,flux_lox,flux_loy,flux_hix,flux_hiy,i,j) = 
                    ( ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j) - ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i-1,j) ) / dx;
            }
        }
    }
    else if (idir == 2) {// flux in y direction
        // double dyflip = 1 / dy;
        for (int j = loy; j <= hiy+1; ++j ) {
            for (int i = lox; i <= hix; ++i ) {
                ARRAY_2D(flux,flux_lox,flux_loy,flux_hix,flux_hiy,i,j) = 
                    ( ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j) - ARRAY_2D(phi,phi_lox,phi_loy,phi_hix,phi_hiy,i,j-1) ) / dy ;
            }
        }
    }
    else {// error
        exit(0);
    }
}
