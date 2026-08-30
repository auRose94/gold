<!-- Agent note: Serialization docs for Gold core. Covers JSON/BSON/CBOR/MsgPack/UBJSON, file pack/unpack, base64, asset pack. -->
# Serialization

<!-- Agent note: This file documents serialization formats, file pack/unpack, base64, asset pack for Gold core. -->

## Binary Codecs

Gold provides self-contained codecs for JSON and binary formats operating directly on `var`/`list`/`object`.

Supported formats:
- JSON – text, pretty printing
- BSON – Binary JSON
- CBOR – Concise Binary Object Representation
- MessagePack – compact binary
- UBJSON – Universal Binary JSON

### API

From `goldjson.hpp`:
```cpp
var jsonParse(string_view data);
string jsonStringify(var data, bool pretty = false);
var bsonParse(string_view data);
binary bsonStringify(var data);
var cborParse(string_view data);
binary cborStringify(var data);
var msgpackParse(string_view data);
binary msgpackStringify(var data);
var ubjsonParse(string_view data);
binary ubjsonStringify(var data);
```

From `file.hpp`:
```cpp
static string serializeJSON(var data, bool pretty = false);
static binary serializeBSON(var data);
static binary serializeCBOR(var data);
static binary serializeMsgPack(var data);
static binary serializeUBJSON(var data);
```

`object` and `list` expose convenience methods:
```cpp
string json = obj.getJSON(true);
binary b = obj.getBSON();
binary c = obj.getCBOR();
binary m = obj.getMsgPack();
binary u = obj.getUBJSON();
```

JSON numbers: positive integers parse as unsigned, negative as signed, decimals/exponents as double. Arrays of uniform numbers 2/3/4/9/16 elements are reconstructed into vec/mat types.

## File I/O

`struct file : public object` provides read/write with write-time caching and parsing.

```cpp
file f(path);
var err = f.load();
var err = f.save();
var wt = f.getWriteTime();
var h = f.hash();
```

Static helpers:
```cpp
var f = file::readFile(path);
var f = file::saveFile(path, string_view data);
var v = file::parseJSON(data);
var v = file::parseBSON(data);
...
```

`file` operators:
- `operator binary()` – load and return binary
- `operator string()` – load and return string
- `operator string_view()` – load and return view

### Base64

```cpp
binary decoded = file::decodeBase64(string_view);
string encoded = file::encodeBase64(binary);
binary decoded = file::decodeDataURL(string_view, string& mimeType);
```

URL-safe alphabet, no padding.

## Asset Pack

Pack multiple files into a single binary container with safety checks.

```cpp
var packed = file::pack(list({
  obj({{"path","z.txt"},{"data",binary({'z'})}}),
  obj({{"path","a.bin"},{"data",binary({1,2,3})}})
}));
var unpacked = file::unpack(packed.getBinary());
```

Pack format:
- Magic `GOLDPAK1`
- File count (uint32 LE)
- Per entry: path size uint32, data size uint64, path bytes, data bytes

Safety:
- Paths must be relative and not contain `..`
- No absolute paths
- Duplicate paths rejected
- Unpack validates header, sizes, and path safety

## Usage Notes

- Malformed input returns `genericError` var, never throws.
- `file::parse*` returns `genericError` on error.
- `asJSON`/`asBSON`/etc on `file` parse `data` field if present.
- Asset pack is for bundling resources, not general compression.

<!-- Agent note: End of serialization docs. -->
