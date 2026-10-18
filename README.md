
<div align="center">
  <img src="logo.png" alt="Neurometrix Logo" width="300"/>
  
  # NEUROMETRIX
  
  **ADVANCED MATHEMATICAL NEUROIMAGING SYSTEM**  
  *QUANTITATIVE STRUCTURAL MEASUREMENT & AGENTIAL INFERENCE ENGINE*
  
  <br/>
  <img src="hackathon.png" alt="AMD Developer Hackathon ACT III" width="600"/>
</div>

---


[![C++20](https://img.shields.io/badge/C++-20-blue?style=for-the-badge&amp;logo=c%2B%2B)()
![AMD ROCm](https://img.shields.io/badge/Optimized%20for-AMD%20ROCm-E31837?style=for-the-badge&amp;logo=amd)](https://www.amd.com/en/graphics/servers-solutions-rocm)

## Project Overview
This is the high-performance mathematical engine of Neurometrix. Written in modern C++20, it is responsible for parsing 4D DICOM volumes (80+slices), reconstructing spatio-temporal data, and computing hemodynamic maps (CBF. CBV, MTT, TMax).

## Why C++?
Python is too slow for 4D medical imaging. By processing the raw bytes and applying deconvolution mathematics natively in C++, we achieve sub-second latency (approx 360ms per 80-slice study).

## Anti-Hallucination
This core is the source of truth. It generates the hard numerical data that the AI agents use. It never uses LLMs; it uses pure physics and mathematics.

## Build Instructions
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
./bin/Release/neuro-4d.exe
```
