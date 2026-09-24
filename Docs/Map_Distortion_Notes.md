# Map distortion and georeferencing status — 2026-09-24

**Blocked before transform.** Working CRS EPSG:32649 is specified in `Data/Coordinate_Contract.json`, but the origin remains unset. The candidate map has no accepted scale, distributed surveyed controls or independent check points. `QA/Map_GCP_Residuals.csv` is a schema with no measurements; no affine/polynomial fit, residual map or georeferenced TIFF is claimed.

The Vrooman plate may be a revision of an older map, and its 1880/1890 date conflict must be resolved. Historic streets cannot be forced onto modern realigned streets. Select fit order only after plotting control distribution and holdout residuals; record rejected points and distortion zones. Georeferencing acceptance follows the proposed budget only after review.
