// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"

#ifndef _GAMING_XBOX
#include <d3dcompiler.h>
#include "dxc/dxcapi.h"
#else
#include <d3d12_xs.h>
#include <d3dx12_xs.h>
#include <dxcapi_xs.h>
#define FindResource(a,b,c) (HRSRC)0
#define D3DCOMPILE_DEBUG                                (1 << 0)
#define D3DCOMPILE_SKIP_VALIDATION                      (1 << 1)
#define D3DCOMPILE_SKIP_OPTIMIZATION                    (1 << 2)
#define D3DCOMPILE_PACK_MATRIX_ROW_MAJOR                (1 << 3)
#define D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR             (1 << 4)
#define D3DCOMPILE_PARTIAL_PRECISION                    (1 << 5)
#define D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT             (1 << 6)
#define D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT             (1 << 7)
#define D3DCOMPILE_NO_PRESHADER                         (1 << 8)
#define D3DCOMPILE_AVOID_FLOW_CONTROL                   (1 << 9)
#define D3DCOMPILE_PREFER_FLOW_CONTROL                  (1 << 10)
#define D3DCOMPILE_ENABLE_STRICTNESS                    (1 << 11)
#define D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY       (1 << 12)
#define D3DCOMPILE_IEEE_STRICTNESS                      (1 << 13)
#define D3DCOMPILE_OPTIMIZATION_LEVEL0                  (1 << 14)
#define D3DCOMPILE_OPTIMIZATION_LEVEL1                  0
#define D3DCOMPILE_OPTIMIZATION_LEVEL2                  ((1 << 14) | (1 << 15))
#define D3DCOMPILE_OPTIMIZATION_LEVEL3                  (1 << 15)
#define D3DCOMPILE_RESERVED16                           (1 << 16)
#define D3DCOMPILE_RESERVED17                           (1 << 17)
#define D3DCOMPILE_WARNINGS_ARE_ERRORS                  (1 << 18)
#define D3DCOMPILE_RESOURCES_MAY_ALIAS                  (1 << 19)
#define D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES   (1 << 20)
#define D3DCOMPILE_ALL_RESOURCES_BOUND                  (1 << 21)
#define D3DCOMPILE_DEBUG_NAME_FOR_SOURCE                (1 << 22)
#define D3DCOMPILE_DEBUG_NAME_FOR_BINARY                (1 << 23)
#endif 

#endif //PCH_H
