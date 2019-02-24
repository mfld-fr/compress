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

Because of small data sizes on the target, compression is performed on the
whole initial sequence of base symbols (= characters as byte codes). This gives
a better symbol ratio, but requires more computation than the algorithms using
a sliding window (these are better suited for long data streams).

The compressor repeatedly scans the sequence to find elementary patterns as
symbol pairs, then replaces the most frequent & asymmetric pair by a secondary
symbol, thus building a binary tree of symbols and a reduced final sequence.

When no more asymmetric pair is duplicated, the compressor reduces the tree,
(including the repeated symbols), then serializes that tree as an indexed table
of words (= dictionary), plus the final sequence.

As this dictionary is static, preceding or embedded in the sequence, it saves
the cost of dynamically rebuild it at decompression.

The table and the sequence are encoded as a bit stream. Base symbols are
serialized as byte codes, while secondary ones are serialized using indexes.

Prefixed coding is prefered to Huffman or arithmetic ones to keep the
decompression cost low, even if less optimal.

Decompression is much simpler. It decodes the bit stream, rebuild the symbol
tree from the table, iterates on the sequence and recursively walks the tree.


STATUS

WORK IN PROGRESS

Already implemented:
- symbol listing
- asymmetric pairing
- repeated symbol in sequence
- tree walking
- bit coding & streaming
- external loopback test

Result:
- already good symbol ratio
- already good decompression time
- acceptable compression time
- but still bad compression ratio

See TODO.txt for next steps.


BENCHMARK

Samples from ELKS project:
https://github.com/jbruchon/elks

- data: kernel data only
- code: kernel code only
- ash: shell (mixed code & data)

Compression ratio:

ENCODING     DATA  CODE   ASH

Initial      6151  43584  51216
B(ase)       6151  43584  51216   Just for testing
R(epeat)B    5650  48716  55948   Not efficient for code
P(refix)B    4840  41659  48955
RPB          4752  43472  50479   Less efficient for code
S(ymbol)E    4851  33809  39794
SI           4547  30853  36307
RSE          3875  35903  41736   Less efficient for code
RSI          x     x      x
PS           x     x      x
RPS          x     x      x

gzip -1      3084  30322  34807
gzip         2999  29230  33660
gzip -9      2999  29216  33652

exomizer     2956  29073  33192


Compression time for ASH (ms):

ENCODING    COMPRESS  EXPAND

B(ase)      6         2
R(epeat)B   -         -
P(refix)B   8         4
RPB         -         -
S(ymbol)E   5604      4
SI          5613      4
RSE         5601      4
RSI         x         x
PS          x         x
RPS         x         x

gzip -1     4         2
gzip        6         2
gzip -9     6         2

exomizer    2146      3
