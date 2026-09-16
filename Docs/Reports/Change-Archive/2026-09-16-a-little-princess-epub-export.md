# A Little Princess EPUB export

## Intent

Package the reviewed 19-chapter Markdown retelling as a readable, navigable EPUB for ebook readers.

## Changed content

- Added [a-little-princess-simple-english.epub](../../Books/a-little-princess-simple-english.epub).
- Converted the manuscript into a title page plus one XHTML page per chapter, with EPUB 3 navigation, package metadata, reading CSS, and a stable spine.
- Kept the Markdown manuscript as the editable source of truth.
- Used a clean typographic title page. No cover image, chapter illustrations, or artwork were added because the user requested EPUB export only and no art assets were requested.

## Validation

- EPUB file created successfully: 387,763 bytes.
- First archive entry is `mimetype`, stored without compression, containing exactly `application/epub+zip`.
- `META-INF/container.xml`, `OEBPS/content.opf`, `OEBPS/nav.xhtml`, and all XHTML pages parse as XML.
- 19 chapter XHTML pages, 19 navigation chapter links, and 19 chapter manifest items found.
- Title page present; 20 XHTML pages total including the title page.
- No image entries or image references are present, matching the text-only export scope.
- Source manuscript remains at 58,741 words across 19 chapters, with every chapter at least 3,000 words.
- `epubcheck` was not installed in the environment, so the package was validated with ZIP invariants and XML parsing instead.

## Illustration

[Open the visual summary](2026-09-16-a-little-princess-epub-export.svg).
