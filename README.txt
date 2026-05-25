PURPOSE

This project features a generic compressor / decompressor, in standard C langage
for best performance and portability.

The compressor is intended to run on a host with standard resources (development
PC). The decompressor is in turn intended to run on a target with limited
resources (embedded system).

The main goal is to save storage space on the target, by compressing at most the
read-only data on the host, and to decompress it on the target at the lowest
cost, with a limited impact on the load time.

Inspired by the famous & venerable Exomizer:
https://github.com/bitshifters/exomizer


DESIGN

Because of small data sizes on the target, compression is performed on the
whole initial sequence of base symbols (as byte codes). This gives a better
symbol ratio, but requires more computation than the algorithms using a
sliding window like LZ* (these are better suited for long data streams).

The compressor repeatedly scans the sequence to find elementary patterns as
symbol pairs, then replaces the most frequent & asymmetric pairs by derived
symbols, thus building a binary tree of symbols and a reduced final sequence.

When no more asymmetric pair is duplicated, the compressor reduces the tree,
(including the repeated symbols), then serializes that tree plus the final
sequence in a bit stream.

For some algorithms (SE, SI), the encoding cost of the serialization is
computed and an optimization loop selects the symbols that are worth to be
defined in the dictionary.

As this dictionary is static, preceding (SE) or embedded (SI) in the final
sequence, it saves the cost of dynamically rebuild it at decompression.

Predefined prefixed coding is prefered to Huffman or arithmetic ones to keep
the decompression cost low, even if less optimal.

Decompression is much simpler, so easy to implement in the target. It decodes
the bit stream, rebuilds the symbol tree, iterates on the final sequence and
recursively walks the tree to output the initial sequence.


STATUS

WORK IN PROGRESS

Already implemented:
- symbol sorting & listing
- asymmetric pairing
- repeated symbol in sequence
- tree walking
- bit coding & streaming
- encoding cost computation

Result:
- already good symbol ratio
- already good decompression time
- acceptable compression time
- but still bad compression ratio

See TODO.txt for next steps.


BENCHMARK

Samples from the ELKS project:
https://github.com/ghaerr/elks

- data: kernel data only
- code: kernel code only
- ash: shell (mixed code & data)

Compression ratio:

ENCODING     DATA  CODE   ASH

Initial      6151  43584  51216
B(ase)       6151  43584  51216   Just for testing
R(epeat)B    5647  48713  55944   Not efficient for code
P(refix)B    4840  41659  48955
RPB          4752  43472  50479   Less efficient for code
S(ymbol)E    4667  30869  36998
SI           4601  30386  36242
RSE          4685  41604  48388   Less efficient (repeat encoding too costly)
RSI          x     x      x
PS           x     x      x
RPS          x     x      x

gzip -1      3084  30322  34807
gzip         2999  29230  33660
gzip -9      2999  29216  33652

exomizer     2956  29073  33192


Compression time for ASH (ms):

ENCODING    COMPRESS  EXPAND

B(ase)      2         0.217
R(epeat)B   3         0.660
P(refix)B   2         0.767
RPB         3         0.785
S(ymbol)E   1719      0.764
SI          1702      0.661
RSE         1738      1.166
RSI         x         x
PS          x         x
RPS         x         x

gzip -1     4         2
gzip        6         2
gzip -9     6         2

exomizer    2146      3
