# Oishii

A powerful and performant C++ library for endian binary IO.
Pronounced oi-shī, from Japanese おいしい--"delicious".

## Reading files

Basic reader functionality has been provided, as well as several layers of compile-time abstraction.

```cpp
// Reads two big-endian ulongs (representing minutes and seconds)
Result<f64> ReadMinutes(std::string_view path) {
  auto reader = TRY(oishii::BinaryReader::FromFilePath(path, std::endian::big));
  u32 minutes = TRY(reader.tryRead<u32>());
  u32 seconds = TRY(reader.tryRead<u32>());
  return static_cast<f64>(minutes) + static_cast<f64>(seconds) / 60.0;
}
```

```cpp
// Writes as two big-endian ulongs
void WriteMinutes(std::string_view path, f64 minutes) {
	oishii::Writer writer;
	writer.setEndian(std::endian::big);
	writer.write<u32>(static_cast<u32>(round(minutes, 60.0));
	writer.write<u32>(static_cast<u32>(fmod(minutes, 60.0) * 60.0);
	writer.saveToDisk(path);
}
```

```cpp
// Reader breakpoint
void ReadBP(std::string_view path) {
	auto reader = oishii::BinaryReader::FromFilePath(path, std::endian::big).value();
	reader.add_bp<u32>(0x10);
	
	// Trigger BP
	reader.seekSet(0x10);
	u32 _ = reader.tryRead<u32>(0).value();
}
```

```
test.bin:0x10: warning: Breakpoint hit
        Offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
        000010  53 45 43 31 CD CD CD CD CD CD CD CD CD CD CD CD SEC1............
                ^~~~~~~~~~~                                     ^~~~
```

```cpp
// Writer test case
void Test() {
	oishii::Writer writer;
	writer.setEndian(std::endian::big);

	std::vector<u8, 4> expected{0x12, 0x34, 0x56, 0x78};
	writer.attachDataForMatchingOutput(expected);

	// Error if does not match
	writer.write<u32>(0x1234'5678);
}
```

Scope-based jumping forms the most basic level. No runtime cost is incurred.

```cpp
using namespace oishii;

void readFile(BinaryReader& reader)
{
	// Read some members
	{
		Jump<Whence::Current> scoped(reader, reader.read<u32>());

		// At end of scope, reader will automatically seek back
	}
	// Continue to read other data
}
```

This is equivalent to:

```cpp
using namespace oishii;

void readFile(BinaryReader& reader)
{
	// Read some members
	u32 section2_offset = reader.read<u32>();
	const auto back = reader.tell();

	reader.seek<Whence::Set>(section2_offset);
	// Read some members

	reader.seek<Whence::Set>(back);
	// Continue to read other data
}
```

### Invalidity tracking

By default, a light stack is updated on dispatch entry/exit, tracking the current reader position.
When the reader signals an invalidity, a stack trace is printed. This has been designed to be incredibly easy to understand and debug.
For example, let's rewrite our reader above to expect a section identifier, `SEC2`:
```cpp
READER_HANDLER(Section2Handler, "Section 2 Data", DecodedSection2&)
{
	reader.expectMagic<'SEC2'>();
	// Read data
}
```
If this expectation fails, the following will be printed.
```
test.bin:0x10: warning: File identification magic mismatch: expecting SEC2, instead saw SEC1.
        Offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
        000010  53 45 43 31 CD CD CD CD CD CD CD CD CD CD CD CD SEC1............
                ^~~~~~~~~~~                                     ^~~~
                In Section 2 Data: start=0x10, at=0x10
                In <root>: start=0x0, at=0x0
                        Offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
                        000000  00 00 00 10 CD CD CD CD CD CD CD CD CD CD CD CD ................
                                ^~~~~~~~~~~                                     ^~~~
```
Note: On Windows, error output will be colored (warnings will be yellow, errors red, and underlines green).

Note: expectMagic<magic> is a wrapper for this:
```cpp
if (read<u32, EndianSelect::Big>() != magic)
	signalInvalidityLast<u32, MagicInvalidity<magic>>();
```
Invalidities adhere to a simple interface and can be easily extended for application-specific needs.

TODO: document other reader features.


## Writing files
All of the basic IO features of reading are supported for writing. Scope based jumping may be used for writing as well.

For more sophisticated writing, a model based on how native applications are built. While writing a file, the user will label data and insert references. Then, the user will instruct the reader to link the file, resolving all references.
This labeling and referencing is done through a hierarchy of nodes (data blocks). Nodes will declare linking restrictions, such as alignment, and when called, will supply child nodes. The linker will recurse through the nodes, filling a layout. Once this layout is filled, the linker may reorder data to be more space efficient or respect linking restrictions. Then, the linker will iterate through this layout, calling the relevant serialization events on the specified nodes, accumulating a list of references to resolve. Once done, the linker will resolve all references and return control to the user.

References may either be in-memory pointers, or namespaced string identifiers. Each node exists in the namespace of its parent, and follows namespacing rules based on C++.
1) A node may reference one of its siblings or children without supplying its own namespace / the namespace of its parent.
2) A node may reference nodes beyond its immediate surroundings by providing a relative or absolute path.

The benefit of supporting this as well as node pointers for identifiers is that data that does not exist yet may be referenced. For example, the actual data block nodes for the rest of a file may not exist at the time the header is being serialized.

Back to our collision example, writing this could be expressed like so:
```cpp
// Data offset 1 indexed, store pointer 0x10 early
writer.writeLink<s32>(Link{ // The linker will resolve this to the distance between hooks
	Hook(*this), // Hook the position of this node in the file
	Hook("TriangleBuffer", DataBlock::Hook::Begin, -0x10) // Hook the beginning of the TriangleBuffer, minus our translation.
	});
```

### Linker Maps

The linker can optionally output a linker map, documenting node positions and ends, as well as hierarchy and linker restrictions. This can be quite useful for debugging the file itself.
This data can also be used to visualize space usage in a file.

TODO: Add example linker map.

