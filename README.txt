PURPOSE

This project features a generic compressor / decompressor, in standard C langage
for best performance and portability.

The compressor is intended to run on a host with standard resources (development
PC). The decompressor is in turn intended to run on a target with limited
resources (embedded, IoT).

The main goal is to save storage space on the target, by compressing at most the
read-only data on the host, and to decompress on the target at the lowest cost,
for a limited impact on the load time.

A secondary goal is to compress and decompress on the target some limited amount
of read-write data, keeping the lowest cost but having a valuable ratio.

Inspired by the famous & venerable Exomizer:
https://github.com/bitshifters/exomizer


DESIGN

Because of small data sizes, compression is performed on the whole input
sequence of initial symbols (= byte codes). This gives a better ratio, but
requires more computation than the sliding window algorithms (the latest
are better suited for long data streams).

The compressor recursively scans the sequence to find elementary patterns as
symbol pairs, and replaces the most frequent & asymmetric pairs by secondary
symbols, thus building a binary tree of symbols as a dictionary.

When no more asymmetric pairs are duplicated, the compressor reduces that
tree and serializes it as a table, followed by the final sequence.

Therefore the dictionary is static, saving the cost of dynamically
building it at decompression.

Initial codes are serialized as byte codes, while secondary ones are
serialized using table index. Groups of a repeated symbol are reduced
to (count, symbol) couples, in both the table and the sequence.

The table and the sequence are then encoded as a bit stream. Prefixed coding
is prefered to Huffman or arithmetic ones to keep the decompression cost low,
even if less optimal.

Decompression is much simpler. It decodes the bit stream, rebuild the symbol
tree from the table, iterates on the sequence and recursively walks the tree.


STATUS

Still WIP:
- data stream implemented
- asymmetric pairing implemented
- repeated symbol implemented (frame only)
- external loopback test passed
- already good ratio before recoding
- but still bad after recoding


See TODO.txt for next steps
