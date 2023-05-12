# Protobuf printer

Provides easier tooling for printing protos in human-readable formats.



```
> cat mypb.bin | printproto
name: "My Proto"
value: 131
data {
  flags: 255
}
```

```
> printproto -f mypb.bin
```

# Descriptors

By default printproto will first look for a pre-compiled descriptor set named "..."
```
> cat mypb.bin | printproto -i mydescriptor.bin
```

Descriptor sets can be read from a given path.
A dot at the end of the path will signal that the entire source tree
should be parsed, equivalent to `./**/*.proto`.
```
> cat mypb.bin | printproto .
```

Descriptor sets can be read from a given path.
```
> cat mypb.bin | printproto proto//. src//google/protobuf/.
```


# Masking
See the documentation for FieldMask at
https://protobuf.dev/reference/java/api-docs/com/google/protobuf/FieldMask
```
> cat mypb.bin | printproto --fieldmask 'path: "a.b" path: "a.b.c"'
```