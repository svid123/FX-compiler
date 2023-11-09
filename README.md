# FX-compiler
Lightweight **HLSL Effect** precompiler based on EBNF and **DXC** for DX12. Please note this is not legacy DX11 'fxc' compiler and it is not compatible with ID3DXEffect. The purpose of this library is to aid precompilation of Microsoft DirectX-traditional effect syntax for cutsom backend. **FXSys** produces parsed techniques, states, as well as compiled bytecode for pass-groups with help of DXC.

Target platform Win-32/64, MSVS2022<br/>
**FXSys** - precompiler project itself<br/>
**FXComp** - console compiler project<br/>
**dxc** - dxc binaries<br/>
