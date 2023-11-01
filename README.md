# FXSys
HLSL Effect precompiler based on EBNF and DXC

Currently provided sources and project are intended for MSVS2022.
Basically it translates FX HLSL into raw HLSL source with removed parts of non-standard HLSL code and adds macro definitions for HLSL DXC compiler
Compiler provides object SFXCode with compiled DX bytecode and additional render state settings such as: BlendState, RasterState, DepthstencilState, as well as definitions in Microsoft FX-like manner.
Currently render state objects reflects DX11-like render state objects (i.e. no conservative rasterizer in RasterDesc etc.)
Additionaly it provides global definitions per pass, definition-ranges per pass with automatically built variants of passes for each value of definition range

Extensions of standard HLSL are
1) Support for 'enum' keyword (translates values into macro definitins, enum)
2) Precompiler collects global initializers of constants and sampelers (and removes from HLSL for DXC).
3) Has it's own C preprocessor with supported keywords:
   **#include
   #define
   #if
   #ifdef
   #else
   #elif
   #endif
   #pragma** (once, far_extern)
     Supported built-in macros:
   __LINE__
   __FILE__
   __DATE__
   __TIME__
   Allows bypass of preprocessor keywords for DXC using ## for keyword (like ##ifdef)
5) Has internal constant-expressions parser for preprocessor #if keyword and all the FX entities (i.e. DepthBias=1+0.5*2)
   
