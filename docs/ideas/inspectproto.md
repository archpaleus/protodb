# Protobuf Inspector

Tool for inspecting binary-encoded protobuf data.

## Functions

### Explain / Inspect

Describe the wire data for a binary-encoded proto.
If no message type is provided then the data will simply describe
the wire types.
The tool displays the offest of every tag, along with the bytes that were read
for the tag and field value. If the tag specifies a length-ecoded type, then the
bytes will show the parsed length, but not the data itself.

```
cat mypb.binproto | inspectproto
000000                [0a e0 0a]    1 : <message>:  (1376 bytes)
000003                   [0a 21]      1 : <string> = "hankings/stars/stars_common.proto"
000026                   [12 0e]      2 : <string> = "hankings.stars"
000036                [22 ff 01]      4 : <message>:  (255 bytes)
000039                   [0a 0a]        1 : <string> = "starsNames"
000045                   [12 12]        2 : <message>:  (18 bytes)
000047                   [0a 04]          1 : <string> = "name"
00004d                   [18 01]          3 : <varint> = 1
```

A type can be provided to give better information how to present the message. By default
we will have descriptor.proto available and all google well known types.

```
cat mypb.binproto | inspectproto google.protobuf.FileDescriptorSet
000000                [0a e0 0a]    1 : message FileDescriptorProto file:  (1376 bytes)
000003                   [0a 21]      1 : string name = "hankings/stars/stars_common.proto"
000026                   [12 0e]      2 : string package = "hankings.stars"
000036                [22 ff 01]      4 : message DescriptorProto message_type:  (255 bytes)
000039                   [0a 0a]        1 : string name = "starsNames"
```

When a descriptor database is given, we will always make a best guess regarding the
protobuf type, and if found we'll use that.

```
cat mypb.binproto | inspectproto -i descriptor_set.bin
000000                [0a e0 0a]    1 : message FileDescriptorProto file:  (1376 bytes)
000003                   [0a 21]      1 : string name = "hankings/stars/stars_common.proto"
000026                   [12 0e]      2 : string package = "hankings.stars"
000036                [22 ff 01]      4 : message DescriptorProto message_type:  (255 bytes)
000039                   [0a 0a]        1 : string name = "starsNames"
...
```

The tooling can inspect a snippet of binary data starting at an offset.

```
cat mypb.binproto | inspectproto --offset 0x36
000036                [22 ff 01]      4 : message DescriptorProto message_type:  (255 bytes)
000039                   [0a 0a]        1 : string name = "starsNames"
...
```

The tooling can inspect a range of binary data

```
cat mypb.binproto | inspectproto 0036..00135
000036                [22 ff 01]      4 : message DescriptorProto message_type:  (255 bytes)
000039                   [0a 0a]        1 : string name = "starsNames"
...
```

A sub-messages contents can be extracted and written to a separate file.  
This will write out the 255 bytes shown in previous examples to a separate file
named `descriptor.proto`.

```
cat mypb.binproto | inspectproto extract 0036 >descriptor.binproto
```

- There are two options here:

* extract the field data, exclusive for run-length delimited size
  - when a message field is extracted we can read that data again using that
    message type; this is the most common use case.
* extract the tag-field combo and write it to the output.
  - this data will not be parsable unless there is a defined type that "wraps" this data.
    for example: FileDescriptorSet wraps a repeated FileDescriptorProto so we can
    extract a FileDescriptorProto and read it as FileDescriptorSet.

If skip invalid is given, then the scan will continue until the first valid data is found.
Note that most random data can actually parse as valid. Given a message type we can easily check if the
field taps match our expectations and skip ahead until we find valid data.
very good validation and need to resort to validity checks across a span of
data or multiple bytes. In either case, we need enough data to produce a valid check.

```
cat invalid_data mypb.binproto | inspectproto --skip_invalid
000000                [0a e0 0a]    <invalid data>  (26 bytes)
00001a                [0a e0 0a]    1 : message FileDescriptorProto file:  (1376 bytes)
```

- We need an algoritm for determining what is valid. A rudimentary check could
  be satisfying one of the two criteria:
  - Parsing at least N tag-field combinations in a row.
    - Run-lenght fields will be scanned for fields, and if valid will add to the the count
    - N=2 is the most flexible, N=4 would be fairly restrictive
  - Requring at least N bytes to parse as valid protobuf wire data.
    - Two fields at a minimum is 4 bytes. But average would likely be 8 bytes or more; strings skew this number much higher.
    - We'd probably want something like 16 bytes. The larger the window size the more expensive the checks.
      inspecting

# Args

-i, --descriptor_set_in
-g, --guess
-G, --noguess

-x, --extract <range>

# Commands

```
inspectproto
inspectproto extract
inspectproto guess
```
