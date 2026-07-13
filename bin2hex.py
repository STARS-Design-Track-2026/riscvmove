#!/usr/bin/env python3
# bin2hex.py
# Converts a raw binary file to a 32-bit hex representation for $readmemh

import sys

def mix32(i):
    # 32-bit integer finalizer (murmur3-style) -- has no small-period
    # structure, unlike a constant or a linear sequence, so every 256-word
    # chunk of padding produced from consecutive indices is bit-distinct
    # from every other chunk. Needed so icebram (which treats two
    # identical 256-word chunks as an unresolvable ambiguity) can locate
    # and replace this region. See run_c target in Makefile.
    x = i & 0xffffffff
    x ^= x >> 16
    x = (x * 0x7feb352d) & 0xffffffff
    x ^= x >> 15
    x = (x * 0x846ca68b) & 0xffffffff
    x ^= x >> 16
    return x

def bin_to_hex(bin_path, hex_path, max_words=512, pad_mode="nop", salt=0):
    try:
        with open(bin_path, "rb") as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: {bin_path} not found.")
        sys.exit(1)

    words = []
    # Read 4 bytes at a time
    for i in range(0, len(data), 4):
        chunk = data[i:i+4]
        # Pad with 0s if chunk is smaller than 4 bytes
        if len(chunk) < 4:
            chunk = chunk + b'\x00' * (4 - len(chunk))
        # Convert little-endian bytes to 32-bit word
        word = (chunk[3] << 24) | (chunk[2] << 16) | (chunk[1] << 8) | chunk[0]
        words.append(word)

    # Pad up to max_words. Using NOP is safer for empty memory areas so that
    # if the PC jumps there, it executes NOPs. The "unique" mode instead
    # pads with a non-repeating pattern -- required for icebram swapping
    # (see mix32 above), and safe here because this region is otherwise
    # untouched stack/unused space that gets overwritten before any real use.
    #
    # salt shifts the index fed to mix32 so that two *different* memories
    # padded independently (e.g. imem and dmem, both called with the same
    # max_words=256-word column size and both starting their padding loop
    # at a small index) can never produce identical padding words at the
    # same position. Without it, imem's first 256-word column and dmem
    # (also 256 words) both compute mix32(i) for whatever tail of indices
    # past their real content is left, and since that's the *same* i for
    # both, they emit bit-identical padding there -- which icebram then
    # reports as an unresolvable "Conflicting from pattern" between the two
    # regions instead of a clean, unique match. Callers must pick disjoint
    # salt ranges per memory (see Makefile) so mix32(i + salt) can never
    # collide across them.
    n = len(words)
    while len(words) < max_words:
        if pad_mode == "unique":
            words.append(mix32(len(words) + salt))
        else:
            words.append(0x00000013)

    # Write to hex file
    with open(hex_path, "w") as f:
        for word in words[:max_words]:
            f.write(f"{word:08x}\n")

    print(f"Successfully converted {len(data)} bytes to {hex_path} ({max_words} words).")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 bin2hex.py <input.bin> <output.hex> [max_words] [pad_mode] [salt]")
        sys.exit(1)

    max_w = 512
    if len(sys.argv) > 3:
        max_w = int(sys.argv[3])

    mode = "nop"
    if len(sys.argv) > 4:
        mode = sys.argv[4]

    pad_salt = 0
    if len(sys.argv) > 5:
        pad_salt = int(sys.argv[5], 0)

    bin_to_hex(sys.argv[1], sys.argv[2], max_w, mode, pad_salt)
