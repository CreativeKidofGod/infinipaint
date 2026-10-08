# InfiniPaint Custom: plan and reasons

Fork: CreativeKidofGod/infinipaint (based on ErrorAtLine0/infinipaint, GPL-3.0).
Owner: Levi. He tests every phase on Windows before the next one starts.

Read this first in any new session. It explains what to build, and why, so
choices stay true to what Levi asked for.

## How to work with Levi

- Plain, simple words. Short updates: DONE / YOU NEED TO / WATCH OUT / NEXT.
- Be ruthless about flagging anything he might forget: risks, setbacks, to-dos.
- All coding and building happen in the cloud or GitHub Actions. Nothing is built on his PC.
  He only downloads the portable zip and tests it.
- Stop after each phase until he confirms the build works.

## Style rules

- **All tool, button, menu and setting names use Title Case** (e.g. "Bring To Front Of Layer"). Levi's choice, 2026-10-08.
  Explanatory sentences and questions stay in normal sentence case. New features must follow this.

## Goals behind everything

1. **One canvas can hold a whole semester or season of life.** Centralize everything in one sandbox.
2. **Simple for anyone.** Each feature is one button, not a menu.
3. **Light on storage and memory, without losing zoom sharpness.**
   - Keep things as vector (shapes) whenever possible.
   - Shrink media when it's added. Store each media file once.
   - Load only what is on screen.
4. **Hard to lose work by accident.**
5. **Possibly sold later.** GPL allows selling, but the source code must be shared.
   Only use code or libraries whose licenses allow that (no BUSL, no AGPL-only libs).
   Example: Inkternity (BUSL-1.1) has good ideas, but its code must not be copied.

## Phase 0: build pipeline + own identity (done, see commit)

- **GitHub Actions builds a portable Windows zip.** Why: nothing gets installed or built on Levi's PC.
- **Conan cache is saved even when a later step fails.** Why: the first Skia build takes 1–2 hours, and losing it to a small compile error would waste that time.
- **The workflow checks for missing DLLs.** Why: so he never gets a zip that crashes on launch.
- **Settings use their own folder ("infinipaint-custom").** Why: so it never touches the official app's settings.
- **The display version is "0.6.1-custom.N", but the internal version stays "0.6.1".**
  Why: config.json and the update checker parse the version as numbers, and a suffix would break settings loading.
- **NOTICE, window title and About screen mark this as a modified, unofficial build.** Why: GPL requires it, and honesty if sold.
- **WARNING:** the portable build keeps config, and the default save folder, next to the exe.
  Replacing the app folder can delete canvases saved there. Warn Levi before every update until Phase 2 fixes it.

## Phase 1: locked layers (done, confirmed by Levi on build -9, 2026-10-08)

- **Four layers at the top of the list, bottom to top: Base, Overlay, Calendar, Writing.**
  They are normal layers recognized by their exact name, so canvases still open in official InfiniPaint.
  Why: no save-format change until Phase 2.
- **New canvases start with these four. Old canvases get any missing ones added when opened.**
  Old content stays where it was (e.g. "First Layer").
- **Routing:** while one of the four is selected, strokes, text, lines and shapes go to Writing.
  Images go to Overlay, unless Base is selected (then they stay on Base). The app switches to the layer the item went into.
  Why: Levi picked "nothing lands in the wrong layer". "Large vs small" can't be detected reliably, so
  images default to Overlay, and boards go to Base by selecting Base first or with "Move to layer".
- **Layers Levi makes himself are never redirected.** Renaming one of the four turns it into a normal layer.
- **Calendar gets nothing automatically yet** (the live calendar is item 9). Use "Move to layer".
- **"Move to layer" buttons** appear under Stroke Color when something is selected with the rectangle or lasso select tools.
  One undo step undoes the move.
- **Levi's additions (2026-10-08):**
  - Dropping a file or pasting an image asks "Put it on which layer?" (Base / Overlay / Calendar / Writing / Cancel).
    Why: so nothing has to be moved after hours of writing on top of it.
    Asked every time. If a layer Levi made himself is selected, it is offered too ("Current layer").
  - Moving keeps the exact position, size and order. It is not a manual copy/delete. One undo step.
    "Move to ... layer" is also in the right-click menu of a selection (Edit, Select and Pan tools).
  - With the Edit tool, one click on any image, file or text selects it, even if it's on another locked layer
    (the app switches to that layer). It checks the current layer first, then Writing, Calendar, Overlay, Base.
  - Moving lines, shapes or text to any layer other than Writing asks "Are you sure?" (Yes, Move / Cancel) first.
    Why: those belong on Writing, so moving them off it is usually a mistake.
  - Putting pictures/files on Writing or Calendar also asks "Are you sure?", both when inserting
    (Yes, Put It There / Go Back) and when moving. Base and Overlay never ask.
  - Pasting an image already exists in InfiniPaint: Ctrl+Shift+V, or right-click > Paste Image.
- Deleting one of the four layers deletes what's on it (Undo brings it back). Routing then uses the current layer,
  its Move To / insert buttons disappear, and it comes back empty next time the canvas is opened.
  Open "Are you sure?" questions about a deleted layer close safely.
- "Are you sure?" and Move To only cover the four locked layers. Layers Levi makes have no Move To button yet.
- Build: one zip instead of a zip in a zip, newer GitHub build tools, and tags starting with "v" publish a Release that never expires.

## Planned features (in order)

1. **Layers**
   - Locked layer types, so nothing lands in the wrong layer. Bottom to top:
     base (large images) → overlay (smaller images) → calendar → writing (handwriting, text, lines, shapes).
   - Select or lasso something, then "move to layer". It works like the existing Stroke Color button in DrawingProgramSelection.
   - Note: changing stroke color on selection already exists. Check that it works well and is easy to find.
2. **Save format + autosave + last 3 backups.**
   - Backups store only what changed, and media is stored once (not duplicated per backup).
   - A canvas folder outside the app folder and outside OneDrive.
3. **Delete protection.**
   - Windows blocks Explorer from deleting canvas files only (deny-delete on those files).
   - The app itself can still save, move and delete them.
   - Why: Levi has accidentally deleted canvases. It must not affect any other files.
4. **Smart import.**
   - PDFs: currently not supported at all (they show as a file icon). Import pages sharp, as vector, using a sell-friendly library such as pdfium.
   - SVGs: the current SkSVGDOM reader drops fonts and CSS styles, so files show blank or wrong. It also re-renders the whole DOM every frame, so it's slow. Convert text to paths and clean up the file on import.
   - Cache a snapshot of each import for speed, and redraw it sharp when zoomed.
   - Why: Levi's "sharp board" workflow. Lecture PDFs are tiled into one big vector board that stays crisp at any zoom.
   - Decided: PDF is the main format for boards, not SVG.
     - It keeps real, searchable text with embedded fonts.
     - It's about 5x smaller (test calendar: PDF 167 KB vs SVG 898 KB).
     - It looks identical everywhere.
   - PDF import offers two choices on drop:
     - "As board": all pages tiled with border lines, like his PNG boards.
     - "Separate pages".
   - Keep the PDF text available to Ctrl+F.
   - Tested 2026-10-06: an SVG board with text converted to paths imports and renders correctly in his current build.
     A PDF dropped onto the canvas shows only a file icon.
   - Checked 2026-10-08 (Windows build): PNG, JPG, WEBP, GIF, BMP and ICO show as pictures. SVG shows, with the limits above.
     PDF, HEIC (iPhone photos), AVIF, TIFF, JPEG XL, Word/PowerPoint and video show only a file icon.
     Add HEIC/AVIF here too if they're easy and sell-friendly (Levi may paste phone photos).
   - **Snip tool (Levi, 2026-10-08):** select part of a PDF page or image on the board and copy it as its own
     object to move, edit or reuse elsewhere. PDF snips stay vector (sharp at any zoom); image snips keep the
     original pixels (crop, no quality loss).
   - Idea to decide later: on import, offer "trace to vector" for simple graphics (diagrams, logos, text) so they
     zoom sharply. Photos can only be AI-upscaled, which adds made-up detail.
     Levi (2026-10-08): he mostly imports PDFs and simple diagram images, so tracing diagrams is the useful case.
5. **Export a region as PDF** (SVG/PNG/JPG/WEBP export already exist; Skia has a PDF backend).
6. **Ctrl+F search.**
   - Covers typed text, PDF text, and handwriting (Windows' built-in, offline handwriting recognition).
7. **From GPL forks** (code can be borrowed):
   - Pen smoothing, lasso fill, dashed lines/arrows, and lock against accidental drawing: alexiokay/infinipaint-Custom.
   - Paint bucket: SquireofNowhere/infinipaint.
8. **Big-canvas tools.**
   - Bookmarks become an easy table of contents with jump links (bookmarks already exist).
   - Merge down.
   - "Sharp flatten": a layer becomes one object that is stored as vector and shown from a cached snapshot. It has an Unflatten button, and the original is kept in the file but not loaded.
     Why: huge canvases stay fast without going blurry.
9. **Live calendar.**
   - A movable, resizable box on its own calendar layer that refreshes itself from a calendar link (e.g. .ics).
   - Notes written on top stay on the writing layer, so updates never erase them.
   - Open question: the calendar source must be a real link the app can read.
10. **Videos.**
    - Shrunk on import, play on click, quality follows zoom.
    - A "fit to screen" button instead of true fullscreen.
    - WARNING: large file sizes.

## Before trusting a phase

- Test with a big, realistic canvas: lots of strokes, several PDFs, and images.
- Check that old canvases still open, and that settings and canvases survive an app update.
