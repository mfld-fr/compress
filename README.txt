The compressor runs on a standard PC.

The expander runs on a system with limited resources:
- device: SoC, SBC...
- CPU: 8 / 16 / 32 bits
- memory: 64K / 1M max

The goal is to save storage space on the device.

Inspired by the famous & venerable Exomizer for 6502 architecture:
https://github.com/bitshifters/exomizer

But more generic to be reused for other architectures.

Current status:
- optimal complexity reduction
- internal loopback test OK
- good ratio compared to GZip

TODO:
- RLE pass on final symbols
- symbol tree reduction
- standalone decompression
- bitstream output / input
- code reduction with adaptative Huffman
