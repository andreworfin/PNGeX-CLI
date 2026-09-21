# PNGeXplorer (PNGeX) CLI

[![C](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.cppreference.com/w/c)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**PNGeX** is a lightweight, low-level C utility and command-line tool designed for parsing, inspecting, manipulating, and performing steganography on PNG (Portable Network Graphics) images. PNGeXplorer runs on **POSIX-compliant operating system** and uses `glibc` and `zlib`. PNGeX interacts directly with raw PNG binary streams to directly read and manipulate files.

## Key Features

* **Chunk Inspection & Parsing**: Reads and validates PNG binary structures, extracting metadata from core chunks (`IHDR`, `PLTE`, `IDAT`, `IEND`).
* **CRC-32 Verification**: Implements lookup-table-based CRC-32 integrity checking across chunk types and data payload streams.
* **PNG Steganography Engine** (Only supports 8-bit images):
  * **Truecolor / Grayscale**: Encodes hidden payloads within the Least Significant Bit (LSB) of pixel channel data.
  * **Indexed Color (Type 3)**: Uses a visually lossless identical-color palette pair swapping algorithm to hide bits without altering image aesthetics.
* **Image Composition & Overlay** (Only supports 8-bit images):
  * Pastes smaller PNG images onto larger canvases at target offsets.
  * Merges indexed color palettes dynamically up to 256 colors.
  * Fully unfilters scanline data across all 5 standard PNG filter algorithms (None, Sub, Up, Average, Paeth).

## Building PNGeX

### Requirements
* POSIX-compliant operating system
* `gcc` or `clang` (C99 standard or later)
* `make` build tool
* `zlib` development library
* Criterion (for running test suite)

### Clone the Repository
```bash
git clone https://github.com/andreworfin/pngx-cli
cd pngx-cli
```

### Compilation
To create the executable:
```bash
make clean all
```

Build with debugging symbols enabled:
```bash
make clean debug
```

Run unit test suite:
```bash
bin/png_tests
```

Remove the executable:
```bash
make clean
```

## Command Line Usage

PNGeX uses a clean two-pass argument parser. The `-f <input.png>` flag is required for all operations except help. Using `-h` will disregard all other commands inputted. Note that `[options]` are optional arguments that will default to 0 if not specified. Also note that `-e`, `-d`, and `-m` flags only support 8-bit images.

```text
Usage: bin/png -f png_file [options]
Options:
  -f png_file                                                Input PNG file (required)
  -h                                                         Print this help message
  -s                                                         Print chunk summary
  -p                                                         Print palette summary
  -i                                                         Print IHDR fields
  -e message -o out_file                                     Encode message and write to output file
  -d                                                         Decode and print hidden message
  -m file2 -o out_file [-w width] [-g height]                Overlay another (smaller) file with over an input file at specific width/height
```

### Example Usage

**Inspect PNG Header & Chunk Structures:**
```bash
bin/png -f sample.png -i -s -p
```

**Hide a Secret Message inside a PNG:**
```bash
bin/png -f secret.png -e "Here's a secret" -o output_steg.png
```

**Extract a Hidden Message from a PNG:**
```bash
bin/png -f output_steg.png -d
```

**Overlay Another Image onto a Base Image:**
```bash
bin/png -f background.png -m overlay.png -o merged.png -w 50 -g 100
```

## Directory Structure

```text
.
├── include/
├── src/
│   ├── helper.c
│   ├── main.c
│   ├── png_chunks.c
│   ├── png_crc.c
│   ├── png_overlay.c
│   ├── png_reader.c
│   ├── png_steg.c
│   └── util.c
├── tests/
└── makefile
```

## License

[MIT](https://github.com/andreworfin/pngx-cli/blob/main/MIT) © [@andreworfin](https://github.com/andreworfin)
