# Mathematical & Software Architecture

## 1. Hexagonal Architecture
The core is isolated from infrastructure. The Kafka adapters and PostgreSQL adapters are injected into the domain.
- domain/: Pure physics logic (no external dependencies).
- application/: Use cases (Perfusion calculation).
- infrastructure/: Kafka, Postgres, DCMTK.

## 2. 4D Volume Reconstruction
- Reads 80 DICOM files using DCMTR.
- Aligns temporal frames.
- Applies a Gamma Variate fit to extract the Arterial Input Function (AIF).

## 3. Perfusion Mathematics
- CBF (Cerebral Blood Flow): Calculated via deconvolution.
- CBV (Cerebral Blood Volume): Area under the curve.
- MTT (Mean Transit Time): CBV / CBF.
