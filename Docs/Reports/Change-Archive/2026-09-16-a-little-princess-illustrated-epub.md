# A Little Princess illustrated EPUB

Date: 2026-09-16

## Intent

Add the missing visual layer to the reviewed easy-English retelling and replace the earlier text-only EPUB export with a reader-ready illustrated package.

## Changed behavior

- Added one portrait black-and-white cover with the exact title, subtitle, and author typeset deterministically.
- Added 19 consistent monochrome chapter illustrations, one for each chapter, normalized to grayscale for e-ink-friendly rendering.
- Updated the EPUB builder to embed the cover and chapter images, reference each chapter image from its matching XHTML page, and mark the cover with `properties="cover-image"` in the OPF.
- Preserved the reviewed 19-chapter manuscript, its chapter lengths, continuity fixes, child-friendly style, and duplicate-text review.

## Validation

- EPUB ZIP integrity and CRC checks passed.
- `mimetype` is the first entry, uncompressed, and contains the exact required bytes.
- `container.xml`, `content.opf`, `nav.xhtml`, the title page, and all 19 chapter XHTML files parse successfully.
- The package contains 1 cover image and 19 chapter images; all final image files are grayscale `L` PNGs.
- All 19 chapter pages reference their matching image; the title page references the cover; the OPF marks the cover as `cover-image`.
- Manuscript QA remains clear: 19 chapters, 3,004–3,344 words each, no duplicate long paragraphs, no duplicate 8+ word sentences, and no placeholders.
- EPUBCheck was not installed in the environment; ZIP/XML/package validation was used instead.

## Illustration

[Open the illustrated EPUB change diagram](2026-09-16-a-little-princess-illustrated-epub.svg)

