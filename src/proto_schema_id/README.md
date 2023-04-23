# Proto ID

Provides a way to identify protobuf messages with unique ID which can be transmitted
efficiently over the wire.

```
protoid.Any {
  id: 0x1234567
  bytes: ...
}
```

used to generate a lookup index.

When

Proto ID becomes a lighter weight way to transfer protobufs dynamically


# Challenges with existing google.protobuf.Any

- Serializing full types in google.protobuf.Any is quite larger.
- Wire data can break due to renames; with ProtoID, the naming can change so
  long as the ID is consistent.

