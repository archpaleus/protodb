# Protobuf printer

Provides easier tooling for printing protos in human-readable formats.

Usage:

```
printproto [binary-encoded proto file | -] [[proto schema]] 
```

Reading input from stdin

```
>  printproto - my.proto <my.pb
name: "My Proto"
value: 131
data {
  flags: 255
}
```

Auto-detecting input argument types: any file that doesn't have a .proto suffix is assumed to be a binary-encoded descriptor set.

```
> printproto my.pb my.proto
```

# Descriptors

By default printproto will first look for a pre-compiled descriptor set following a list of well-known names and suffixes.  A descriptor set can be provided from the command line using `-i` or `--descriptor_set_in`.  Multiple descriptor sets can be provided, either by specifying the option multiple times, or by comma-separated filenames.

```
> cat mypb.bin | printproto -i descriptor1.bin,descriptor2.bin
> cat mypb.bin | printproto -i descriptor1.bin -i descriptor2.bin
```

Descriptor sets can be read from a given path.
A dot at the end of the path will signal that the entire source tree
should be parsed, equivalent to `./**/*.proto`.

```
> cat mypb.bin | printproto .
```

Required protobuf schema can be read from a given path.

```
> cat mypb.bin | printproto proto//. src//google/protobuf/.
```

# Masking

See the documentation for FieldMask at
https://protobuf.dev/reference/java/api-docs/com/google/protobuf/FieldMask

```
> cat mypb.bin | printproto --fieldmask 'path: "a.b" path: "a.b.c"'
```

## Advanced Usage

Using shell expansions:

```
printproto proto//**/*.proto myprotodata.pb
```

Printing files in a directory with xargs:

```
ls *.pb | xargs -L 1 printproto proto//**/*.proto
```
