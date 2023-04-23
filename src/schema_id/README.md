# Proto ID

Provides a way to identify protobuf messages with unique ID which can be transmitted
efficiently over the wire.

Serializing and desiralizing
```
MyProto my_proto = ...;
protobunny::Any any;

protobunny::SchemaMap schema_map;
ABSL_CHECK(schema_map.Pack(my_proto, &any));

ABSL_CHECK(schema_map.Unpack(any, &my_proto));
```

Over the wire the data for protobuf::Any is stored efficently with only an ID into
the proto ID schema mapping and the bytes of the message.
```
message_id: 0x1234567
message: ...
```

```
message MyProto {
  option (protobunny.schema).id32 = 0x1234567;

  ...
}
```

When compiling your protos, a schema map is built by looking at all annotated 
messages in a file descriptor set.
```
entry { id32: 0x1234567 message_type: "MyProto" }
```


Proto ID becomes a lighter weight way to transfer protobufs dynamically


# Challenges with existing google.protobuf.Any

- Serializing full types in google.protobuf.Any is quite larger.
- Wire data can break due to renames; with ProtoID, the naming can change so
  long as the ID is consistent.

