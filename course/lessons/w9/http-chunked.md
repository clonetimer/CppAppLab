# W9-A · Incremental HTTP/1.1 Chunked Transfer Decoding

## Start here

Decode one chunked body by following its framing.

1. Read a complete example and identify the hexadecimal chunk size.
2. Parse the size line, then consume exactly that many payload bytes.
3. Verify the following CRLF before reading the next chunk.
4. Feed the same body in fragments, including a split size line and split payload.
5. Recognize the zero-size final chunk and parse trailers.
6. Test invalid sizes, missing CRLF and body-size limits.

**Mental model:** chunked encoding frames the body when its final length is not known in advance. TCP may split every part; the caller retains and extends the accumulated buffer, and the stateless parser examines it on each call.


## Goal

Extend the W8 incremental HTTP parser to decode HTTP/1.1 chunked request bodies without losing TCP stream boundaries. The parser must remain incremental: a partial chunk line, chunk payload, CRLF terminator or trailer block returns NeedMoreData, not a malformed-request error.

## Message-framing rules

The W8 parser already supports strict Content-Length framing. This task adds a deliberately bounded chunked profile:

- Accept exactly one Transfer-Encoding coding: chunked (ASCII case-insensitive, with optional whitespace around the value).
- Reject Content-Length and Transfer-Encoding appearing together. Never guess which framing header wins.
- Reject unsupported codings and coding chains such as gzip, chunked.
- Parse each hexadecimal chunk size, optional visible-ASCII extension text, exactly that many payload bytes, and the following CRLF.
- A zero-sized chunk begins the trailer section. Validate trailer syntax, reject duplicate trailer names and reject framing/routing fields (Content-Length, Transfer-Encoding, Host, Connection, Trailer, Upgrade). Trailers are validated and consumed but are not merged into request headers.
- Enforce the existing 1 MiB decoded-body limit, a 1 KiB chunk-size-line limit and a 16 KiB trailer-section limit.
- Return consumed through the final empty trailer line only. Any pipelined request remains in the input buffer.

This course profile intentionally does not decode transfer-coding chains or expose trailer values to handlers. It does not change the existing Content-Length behavior.

## Chunked wire shape

```text
Transfer-Encoding: chunked

4;name=value\r\n
Wiki\r\n
5\r\n
pedia\r\n
0\r\n
X-Trace: abc\r\n
\r\n
```

The decoded body is Wikipedia. The chunk metadata and trailer block are not part of the body.

## Incremental state machine

1. Read a chunk-size line ending in CRLF. Parse the hexadecimal size with std::from_chars(base=16).
2. If the chunk size is nonzero, verify the cumulative decoded body will not exceed 1 MiB. Wait for the full payload plus CRLF, append exactly the payload bytes, and continue.
3. If the chunk size is zero, parse trailer fields until the final empty CRLF line.
4. Return Complete and the exact number of bytes consumed from the original buffer.
5. If any required bytes are missing, return NeedMoreData with consumed == 0.
6. If framing is ambiguous, malformed or exceeds a limit, return Error.

Never use strlen, line-oriented text I/O or a C string terminator to determine chunk payload length. Chunk data is arbitrary bytes.

## Learner task — w9.http-chunked

**Workspace files**

- include/http_message.hpp
- include/http_request_parser.hpp
- src/http_request_parser.cpp

Implement the chunked framing branch in the parser. Preserve the W8 request model and the ParseStatus / ParseResult contract.

**Fixed acceptance:** 35 platform-owned GoogleTests in course/fixtures/w9/http-chunked. They cover multi-chunk and binary payloads, extensions, trailers, incremental boundaries, pipelining, ambiguous framing, malformed input and resource limits.

```bash
./build/cppapplab start w9.http-chunked "$HOME/cppapplab-work/w9-http-chunked"
./build/cppapplab lesson "$HOME/cppapplab-work/w9-http-chunked"
./build/cppapplab hint "$HOME/cppapplab-work/w9-http-chunked" 1
./build/cppapplab check "$HOME/cppapplab-work/w9-http-chunked"
```

## Explain your design

1. Why must Content-Length and Transfer-Encoding not coexist?
2. Why does a chunk parser need both a decoded-body limit and a metadata-line limit?
3. Why is chunk size measured in bytes rather than characters?
4. Why must the parser wait for the CRLF after every nonzero chunk?
5. Why are framing and routing fields forbidden in trailers?
6. Why should trailers not silently overwrite request headers?
7. Why must consumed stop before the next pipelined request?
8. Why does an incomplete trailer block return NeedMoreData?

## Troubleshooting

Use the fixed parser checks for this library task; no standalone parser application is staged.

- **Fragmented chunks fail or duplicate payload bytes:** inspect the complete buffer passed to each `parse` call. Retain prior bytes while awaiting more input, and recheck split size/data/terminator cases with `NeedMoreData` and zero consumed bytes.
- **Decoded data contains metadata or loses binary bytes:** inspect the hexadecimal size and payload boundary before its CRLF. Recheck binary payloads and embedded CRLF, counting exact payload bytes rather than reading data as lines.
- **The following request disappears or trailers overwrite headers:** inspect the cursor through the zero chunk and final empty trailer line. Recheck consumed-byte and trailer cases, validating duplicate/forbidden fields without merging trailers into request headers.
- **Ambiguous or oversized input is accepted:** inspect Content-Length plus Transfer-Encoding, HTTP version and the coding value. Recheck their rejection cases alongside decoded-body, chunk-line and trailer limits; W9 accepts only the bounded HTTP/1.1 chunked profile.

## Acceptance

- [ ] Single and multiple chunks decode to exact bytes.
- [ ] Chunk extensions are accepted only as bounded visible-ASCII metadata.
- [ ] Empty bodies and valid trailers are handled.
- [ ] Partial size/data/terminator/trailer input is incremental.
- [ ] Content-Length + Transfer-Encoding is rejected.
- [ ] Unsupported transfer-coding chains are rejected.
- [ ] Chunk and trailer metadata limits are enforced.
- [ ] Pipelined bytes remain unconsumed.
- [ ] Existing Content-Length framing remains unchanged.
- [ ] All 35 fixed checks pass.
