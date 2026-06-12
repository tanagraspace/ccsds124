#!/bin/bash
# Generate API documentation from Go source files

OUTPUT_DIR="$1"

if [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <output-dir>"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Start main index page
cat > "$OUTPUT_DIR/index.html" << 'HTMLEOF'
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CCSDS 124.0-B-1 Go Implementation</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; max-width: 900px; margin: 0 auto; padding: 2rem; line-height: 1.6; background: #f8f9fa; }
    h1 { color: #333; border-bottom: 2px solid #00ADD8; padding-bottom: 0.5rem; }
    h2 { color: #00ADD8; margin-top: 2rem; }
    h3 { color: #333; border-bottom: 1px solid #eee; padding-bottom: 0.25rem; }
    .package { background: white; padding: 1.5rem; border-radius: 8px; margin: 1rem 0; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    .package h2 { margin-top: 0; }
    .doc { color: #555; margin: 0.5rem 0; }
    pre { background: #282c34; color: #abb2bf; padding: 1rem; border-radius: 4px; overflow-x: auto; font-size: 0.9em; }
    code { font-family: 'SF Mono', Monaco, 'Courier New', monospace; }
    .func { margin: 1rem 0; padding: 1rem; background: #f1f3f5; border-radius: 4px; border-left: 3px solid #00ADD8; }
    .func-sig { font-family: 'SF Mono', Monaco, monospace; font-size: 0.9em; color: #00ADD8; margin-bottom: 0.5rem; }
    .type { margin: 1rem 0; padding: 1rem; background: #fff8e6; border-radius: 4px; border-left: 3px solid #f0c000; }
    .type-name { font-family: 'SF Mono', Monaco, monospace; font-weight: bold; color: #b58900; }
    .const { background: #e8f5e9; border-left-color: #28a745; }
    a { color: #00ADD8; text-decoration: none; }
    a:hover { text-decoration: underline; }
    .nav { background: white; padding: 1rem; border-radius: 8px; margin-bottom: 2rem; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    .nav ul { list-style: none; padding: 0; margin: 0; display: flex; flex-wrap: wrap; gap: 1rem; }
    .nav li a { padding: 0.5rem 1rem; background: #00ADD8; color: white; border-radius: 4px; display: inline-block; }
    .nav li a:hover { background: #0093b8; text-decoration: none; }
    .citation { background: white; padding: 1.5rem; border-radius: 8px; margin: 1rem 0; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    .citation blockquote { font-size: 0.9em; border-left: 3px solid #00ADD8; padding-left: 1em; margin: 1em 0; color: #555; }
    .citation details { margin: 1em 0; }
    .citation summary { cursor: pointer; color: #00ADD8; }
    .citation pre { background: #f1f3f5; color: #333; font-size: 0.85em; margin-top: 0.5em; }
  </style>
</head>
<body>
  <h1>CCSDS 124.0-B-1 Go Implementation</h1>
  <p>Go implementation of the CCSDS 124.0-B-1 lossless compression algorithm of fixed-length housekeeping data.</p>

  <div class="citation">
    <h2 style="margin-top: 0;">Citation</h2>
    <p>If CCSDS 124.0-B-1 contributes to your research, please cite:</p>
    <blockquote>
      D. Evans, G. Labrèche, D. Marszk, S. Bammens, M. Hernandez-Cabronero, V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022. "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1," <em>Proceedings of the Small Satellite Conference</em>, Communications, SSC22-XII-03. <a href="https://digitalcommons.usu.edu/smallsat/2022/all2022/133/">https://digitalcommons.usu.edu/smallsat/2022/all2022/133/</a>
    </blockquote>
    <details>
      <summary>BibTeX</summary>
      <pre>@inproceedings{evans2022ccsds124,
  author    = {Evans, David and Labreche, Georges and Marszk, Dominik and Bammens, Samuel and Hernandez-Cabronero, Miguel and Zelenevskiy, Vladimir and Shiradhonkar, Vasundhara and Starcik, Mario and Henkel, Maximilian},
  title     = {Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1},
  booktitle = {Proceedings of the Small Satellite Conference},
  year      = {2022},
  note      = {SSC22-XII-03},
  url       = {https://digitalcommons.usu.edu/smallsat/2022/all2022/133/}
}</pre>
    </details>
  </div>

  <div class="nav">
    <ul>
      <li><a href="#package">Package</a></li>
      <li><a href="#types">Types</a></li>
      <li><a href="#functions">Functions</a></li>
      <li><a href="#constants">Constants</a></li>
    </ul>
  </div>

  <div class="package">
    <h2 id="package">Package ccsds124</h2>
    <p class="doc">Package ccsds124 implements the CCSDS 124.0-B-1 lossless data compression algorithm for fixed-length housekeeping data.</p>
HTMLEOF

# Generate documentation for types
cat >> "$OUTPUT_DIR/index.html" << 'HTMLEOF'
    <h2 id="types">Types</h2>

    <div class="type">
      <div class="type-name">type Compressor</div>
      <pre><code>type Compressor struct {
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">Compressor maintains state for CCSDS 124.0-B-1 compression across multiple packets.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func NewCompressor(F int, initialMask *BitVector, robustness, ptLimit, ftLimit, rtLimit int) (*Compressor, error)</div>
        <p class="doc">NewCompressor creates a new compressor with the given parameters. F is the input vector length in bits, initialMask is the optional initial mask, robustness is Rt (0-7), and ptLimit/ftLimit/rtLimit control automatic mode triggers.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (c *Compressor) CompressPacket(input *BitVector, params *Params) (*BitBuffer, error)</div>
        <p class="doc">CompressPacket compresses a single input packet and returns the compressed output.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (c *Compressor) Reset()</div>
        <p class="doc">Reset resets the compressor to its initial state.</p>
      </div>
    </div>

    <div class="type">
      <div class="type-name">type Decompressor</div>
      <pre><code>type Decompressor struct {
    F          int        // Input vector length in bits
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">Decompressor maintains state for CCSDS 124.0-B-1 decompression.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func NewDecompressor(F int, initialMask *BitVector, robustness int) (*Decompressor, error)</div>
        <p class="doc">NewDecompressor creates a new decompressor with the given parameters.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (d *Decompressor) DecompressPacket(reader *BitReader) (*BitVector, error)</div>
        <p class="doc">DecompressPacket decompresses a single compressed packet.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (d *Decompressor) DecompressStream(data []byte, numBits int) ([][]byte, error)</div>
        <p class="doc">DecompressStream decompresses multiple packets from a byte stream.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (d *Decompressor) NewPacketIterator(data []byte, numBits int) *PacketIterator</div>
        <p class="doc">NewPacketIterator creates a streaming packet iterator for memory-efficient decompression.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (d *Decompressor) StreamPackets(data []byte, numBits int) &lt;-chan []byte</div>
        <p class="doc">StreamPackets returns a channel that yields decompressed packets.</p>
      </div>
    </div>

    <div class="type">
      <div class="type-name">type BitVector</div>
      <pre><code>type BitVector struct {
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">BitVector represents a fixed-length sequence of bits stored in 32-bit words with MSB-first bit ordering.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func NewBitVector(length int) (*BitVector, error)</div>
        <p class="doc">NewBitVector creates a new BitVector with the specified length in bits.</p>
      </div>
      <div class="func">
        <div class="func-sig">func NewBitVectorFromBytes(data []byte, length int) (*BitVector, error)</div>
        <p class="doc">NewBitVectorFromBytes creates a BitVector from a byte slice.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) GetBit(pos int) int</div>
        <p class="doc">GetBit returns the bit value (0 or 1) at the given position.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) SetBit(pos int, value int)</div>
        <p class="doc">SetBit sets the bit at the given position to the specified value.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) XOR(other *BitVector) *BitVector</div>
        <p class="doc">XOR returns a new BitVector that is the XOR of this and other.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) OR(other *BitVector) *BitVector</div>
        <p class="doc">OR returns a new BitVector that is the OR of this and other.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) AND(other *BitVector) *BitVector</div>
        <p class="doc">AND returns a new BitVector that is the AND of this and other.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) NOT() *BitVector</div>
        <p class="doc">NOT returns a new BitVector with all bits inverted.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) HammingWeight() int</div>
        <p class="doc">HammingWeight returns the number of '1' bits in the vector.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bv *BitVector) ToBytes() []byte</div>
        <p class="doc">ToBytes converts the BitVector to a byte slice.</p>
      </div>
    </div>

    <div class="type">
      <div class="type-name">type BitBuffer</div>
      <pre><code>type BitBuffer struct {
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">BitBuffer is a variable-length bit buffer for building compressed output.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func NewBitBuffer(capacity int) *BitBuffer</div>
        <p class="doc">NewBitBuffer creates a new BitBuffer with the specified initial capacity.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bb *BitBuffer) AppendBit(bit int)</div>
        <p class="doc">AppendBit appends a single bit (0 or 1) to the buffer.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bb *BitBuffer) AppendBits(value uint64, count int)</div>
        <p class="doc">AppendBits appends multiple bits from a value (MSB first).</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bb *BitBuffer) Size() int</div>
        <p class="doc">Size returns the number of bits in the buffer.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (bb *BitBuffer) ToBytes() []byte</div>
        <p class="doc">ToBytes returns the buffer contents as a byte slice.</p>
      </div>
    </div>

    <div class="type">
      <div class="type-name">type BitReader</div>
      <pre><code>type BitReader struct {
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">BitReader provides sequential bit reading from a byte buffer.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func NewBitReader(data []byte) *BitReader</div>
        <p class="doc">NewBitReader creates a new BitReader from a byte slice.</p>
      </div>
      <div class="func">
        <div class="func-sig">func NewBitReaderWithBits(data []byte, numBits int) *BitReader</div>
        <p class="doc">NewBitReaderWithBits creates a BitReader with an explicit bit count.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (br *BitReader) ReadBit() (int, error)</div>
        <p class="doc">ReadBit reads and returns the next bit.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (br *BitReader) ReadBits(count int) (uint64, error)</div>
        <p class="doc">ReadBits reads and returns the next count bits as a uint64.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (br *BitReader) Remaining() int</div>
        <p class="doc">Remaining returns the number of unread bits.</p>
      </div>
    </div>

    <div class="type">
      <div class="type-name">type Params</div>
      <pre><code>type Params struct {
    NewMaskFlag bool // Force new mask transmission
    ResetFlag   bool // Force full packet transmission
}</code></pre>
      <p class="doc">Params contains per-packet compression parameters.</p>
    </div>

    <div class="type">
      <div class="type-name">type PacketIterator</div>
      <pre><code>type PacketIterator struct {
    // Contains filtered or unexported fields
}</code></pre>
      <p class="doc">PacketIterator provides streaming decompression with an iterator pattern.</p>
      <h4>Methods</h4>
      <div class="func">
        <div class="func-sig">func (it *PacketIterator) Next() []byte</div>
        <p class="doc">Next returns the next decompressed packet, or nil if done or on error.</p>
      </div>
      <div class="func">
        <div class="func-sig">func (it *PacketIterator) Err() error</div>
        <p class="doc">Err returns any error encountered during iteration.</p>
      </div>
    </div>
HTMLEOF

# Generate documentation for functions
cat >> "$OUTPUT_DIR/index.html" << 'HTMLEOF'
    <h2 id="functions">Encoding Functions</h2>

    <div class="func">
      <div class="func-sig">func CountEncode(bb *BitBuffer, A int) error</div>
      <p class="doc">CountEncode implements CCSDS 124.0-B-1 Section 5.2.2, Table 5-1, Equation 9. Encodes positive integers 1 &lt;= A &lt;= 2^16 - 1.</p>
    </div>

    <div class="func">
      <div class="func-sig">func CountEncodeTerminator(bb *BitBuffer)</div>
      <p class="doc">CountEncodeTerminator writes the RLE terminator pattern '10'.</p>
    </div>

    <div class="func">
      <div class="func-sig">func RLEEncode(bb *BitBuffer, input *BitVector) error</div>
      <p class="doc">RLEEncode implements CCSDS 124.0-B-1 Section 5.2.3, Equation 10. Encodes a bit vector using run-length encoding.</p>
    </div>

    <div class="func">
      <div class="func-sig">func BitExtract(bb *BitBuffer, data, mask *BitVector) error</div>
      <p class="doc">BitExtract implements CCSDS 124.0-B-1 Section 5.2.4, Equation 11. Extracts bits from data at positions where mask has '1' bits.</p>
    </div>

    <div class="func">
      <div class="func-sig">func BitExtractForward(bb *BitBuffer, data, mask *BitVector) error</div>
      <p class="doc">BitExtractForward extracts bits in forward order (lowest position to highest). Used for kt component.</p>
    </div>

    <h2>Decoding Functions</h2>

    <div class="func">
      <div class="func-sig">func CountDecode(br *BitReader) (int, error)</div>
      <p class="doc">CountDecode decodes a COUNT-encoded value. Returns the decoded positive integer.</p>
    </div>

    <div class="func">
      <div class="func-sig">func RLEDecode(br *BitReader, length int) (*BitVector, error)</div>
      <p class="doc">RLEDecode decodes an RLE-encoded bit vector of the specified length.</p>
    </div>

    <div class="func">
      <div class="func-sig">func BitInsert(br *BitReader, data, mask *BitVector) error</div>
      <p class="doc">BitInsert inserts bits from reader into data at positions where mask has '1' bits.</p>
    </div>

    <div class="func">
      <div class="func-sig">func BitInsertForward(br *BitReader, data, mask *BitVector) error</div>
      <p class="doc">BitInsertForward inserts bits in forward order (for kt component).</p>
    </div>

    <h2>Mask Operations</h2>

    <div class="func">
      <div class="func-sig">func UpdateBuild(build, change *BitVector) *BitVector</div>
      <p class="doc">UpdateBuild computes B_t = B_{t-1} OR D_t (CCSDS Equation 6).</p>
    </div>

    <div class="func">
      <div class="func-sig">func UpdateMask(mask, build *BitVector, newMaskFlag bool) *BitVector</div>
      <p class="doc">UpdateMask computes M_t based on CCSDS Equation 7.</p>
    </div>

    <div class="func">
      <div class="func-sig">func ComputeChange(prevMask, currMask *BitVector) *BitVector</div>
      <p class="doc">ComputeChange computes D_t = M_{t-1} XOR M_t (CCSDS Equation 8).</p>
    </div>

    <div class="func">
      <div class="func-sig">func HorizontalXOR(input *BitVector) *BitVector</div>
      <p class="doc">HorizontalXOR computes horizontal XOR for mask transmission: result[i] = input[i] XOR input[i+1].</p>
    </div>
HTMLEOF

# Generate documentation for constants
cat >> "$OUTPUT_DIR/index.html" << 'HTMLEOF'
    <h2 id="constants">Constants</h2>

    <div class="type const">
      <pre><code>const (
    MaxRobustness = 7  // Maximum robustness parameter (R_t)
)</code></pre>
      <p class="doc">MaxRobustness defines the maximum allowed robustness parameter value per CCSDS 124.0-B-1.</p>
    </div>
  </div>

  <p style="color: #666; font-size: 0.8rem; margin-top: 2rem;">
HTMLEOF

echo "    Generated on $(date -u '+%Y-%m-%d %H:%M:%S UTC')" >> "$OUTPUT_DIR/index.html"

cat >> "$OUTPUT_DIR/index.html" << 'HTMLEOF'
  </p>
</body>
</html>
HTMLEOF

echo "API documentation generated: $OUTPUT_DIR/index.html"
