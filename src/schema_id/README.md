# Proto Schema ID

Provides a way to identify protobuf messages with unique ID which can be transmitted
efficiently over the wire.

## Serializing and Deserializing
```
MyProto my_proto = ...;
protobunny::Any any;

protobunny::SchemaMap schema_map;
ABSL_CHECK(schema_map.Pack(my_proto, &any));

google::protobuf::Message message;
ABSL_CHECK(schema_map.Unpack(any, &message));

ABSL_CHECK(schema_map.Is<MyProto>(any));
schema_map.Unpack(any, &my_proto);
```

## Protobuf Schema Annotations
```
message MyProto {
  option (protobunny.schema).id32 = 0x1234567;

  ...
}
```

## Wire Format
Over the wire, the data for protobuf::Any is stored efficently with only an
ID into the schema mapping and the bytes of the message.
```
id32: 0x1234567
message: ...
```

## Schema Mapping Data
When compiling your protos, a schema map is built by looking at all annotated 
messages in a file descriptor set.

Mapping as a protobuf message
```
entry { id32: 0x1234567 message_type: "MyProto" }
```

Additionally the mapping can be generated in code.  For example, in C++ we
can generate a constexpr data structure that can be read directly without
incurring any allocations.


# Challenges with existing google.protobuf.Any

- Serializing full types in google.protobuf.Any is quite large.
- Wire data can break due to renames; with ProtoSchemaID, the naming can change so
  long as the Schema ID is consistent.

