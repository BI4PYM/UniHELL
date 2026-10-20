# Third-party components

The UniHELL 2.0.0 release executables are built for
`x86_64-w64-mingw32` with GCC 16.1.0 (WinLibs UCRT, POSIX threads, SEH).
They statically link the following components and runtime code:

| Component | Version | License files |
| --- | --- | --- |
| FreeType | 2.13.3 | `FreeType-LICENSE.txt`, `FreeType-FTL.txt` |
| FreeType BDF driver | 2.13.3 | `FreeType-BDF-LICENSE.txt` |
| FreeType PCF driver | 2.13.3 | `FreeType-PCF-LICENSE.txt` |
| zlib, embedded in FreeType | 1.3 | `ZLIB-LICENSE.txt` |
| HarfBuzz | 8.5.0 | `HarfBuzz-COPYING.txt` |
| HarfBuzz Indic shaping data (`src/ms-use`) | 8.5.0 | `HarfBuzz-ms-use-COPYING.txt` |
| MinGW-w64 runtime and winpthread | GCC 16.1.0 toolchain | `MinGW-w64-RUNTIME-LICENSE.txt` |
| GCC runtime libraries | GCC 16.1.0 | `GPL-3.0.txt`, `GCC-RUNTIME-EXCEPTION.txt` |

The full upstream license texts are stored in the corresponding source
subdirectories under `third_party/`. Font licenses are separate and remain in
`fonts/`.
