# Proto Schema IDs

Provides a unique identifiers for messages within a .proto closure that
enables simpler ways to identify the format of a protobuf message across
the wire.

Using these IDs, you can more succinctly communicate your message type in an
`Any`-style message.   Writing a 64-bit protoid requires 5 bytes of data on
the wire, keeping overhead minimal, especially when compared to a
`google.protobuf.Any` message, which is typically
more than 40 bytes to specify the message type.

In total a `protoid.Any` message has an overhead of 6-11 bytes on top of
the serialized message.

```
import "protoid/any.proto"

pacakge my-package;

message MyMessage {
  option (protoid.id) = { id: 4560029131573256278 }

  ...
}
```

# Using a simplified Any proto

```
package protoid;
message Any {
  # We use higher tag numbers to make distinguishing the message easier.
  # The first four IDs are reserved and always unused.  This makes it
  # easy to rule out any proto as a type mismatch if it's using
  # these field numbers, which is the majority of protos we'll see.
  reserved 1-4;

  // ID may be chosen as either a 32-bit ID or a 64-bit id.
  oneof id {
    # 32-bit ID of the message type.
    fixed32 id32 = 5;

    # 64-bit ID of the message type.
    fixed64 id64 = 6;
  }

  # Binary encoding data of the serialized message.
  bytes message = 7;

  # An optional typename for the message, eg "google.protobuf.Empty".
  # Useful for debugging or when the ID will be unavailable.
  string type_name = 76;
}


protoid_any: {
  id64: 4560029131573256278
  message: ...
}

protoid_any: {
  id32: 3369820246
  message: ...
  type_name: "my.team.Proto"
}
```

Using an AnySet with a repeated field, we can easily write out a large number of
Any objects, which can be easily concatenated together and read later.

```
message AnySet {
  repeated Any = 3;
}
```

```
message AnyArray {
  # All fields must be consistent with Any, except for 'message' which is repeated.

  reserved 1-4;

  // ID may be chosen as either a 32-bit ID or a 64-bit id.
  oneof id {
    # 32-bit ID of the message type.
    fixed32 id32 = 5;

    # 64-bit ID of the message type.
    fixed64 id64 = 6;
  }

  # Binary encoding data of the serialized messages.
  repeated bytes message = 7;

  # An optional typename for the message, eg "google.protobuf.Empty".
  # Useful for debugging or when the ID will be unavailable.
  string type_name = 76;
}
```

```
# Variant should remain identical to `Any` in wire-format.
# Variants are annotated in the schema to limit to a specific
# set of protos that can serialized for the proto.
# Parsing is less restrictive and will deserialize to any type
# that can be found in the mapping.
message Variant {
  reserved 1-4;

  // ID may be chosen as either a 32-bit ID or a 64-bit id.
  oneof id {
    # 32-bit ID of the message type.
    fixed32 id32 = 5;

    # 64-bit ID of the message type.
    fixed64 id64 = 6;
  }

  # Binary encoding data of the serialized message.
  bytes message = 7;

  # An optional typename for the message, eg "google.protobuf.Empty".
  # Useful for debugging or when the ID will be unavailable.
  string type_name = 76;
}

```

```
extend FieldOptions {
  repeated string types = 50000 [retention = RETENTION_SOURCE];
}
```

```
message MyMessage {
  Variant my_field = 1 [(protoid.variants) = {
    type: "google.protobuf.FloatValue"
    type: "google.protobuf.BoolValue"
    type: "google.protobuf.Int32Value"
    type: "google.protobuf.Int64Value"
  }];
}
```

This generates the ProtoIdMap entry that allows the variant to be accessed.
```
// Load the map for the containing message.
// The protoID plugin will generate the C++ template matching the message type
// along wih a custom type for the field, defined in the same namespace.
// The returned type will conform to
// std::variant<FloatValue, BoolValue, Int32Value, Int64Value>
auto variant = ProtoId::Variant<MyMessage, my_field>();

// The type can also be used directly.
ProtoId::Variant<MyMessage, my_field>::type variant;

// Unpack the 'myprotos' field from a parsed message.
MyMessage my_message = ParseFromString(data);
auto message = ProtoId::Variant<MyMessage, my_field>::Unpack(data.my_field);
// OPTION: shorter form that reduces duplicate on my_field
auto message = ProtoId::Variant::Unpack(data, my_field);

// Unpack to a Message.  If the message is not the right type then unpacking will fail.
bool ok = variant.Unpack(data, my_field, &message);

// Given a std::variant<> with types matching data.myprotos we can
// repack it to the field.
variant.Pack(message, &data.myprotos);
```

# Variant Implementation for C++

```
// From the type we can get the descriptor
const Descriptor* d = MyMessage::descriptor()
int field = MyMessage::kTextFieldNumber;
```

```
namespace mypackage {

template<typename Message, VariantType>
struct VariantField {
  using message = Message;
  using type = VariantType;
};
// this needs to be within a "message" type so that it doesn't collide with
// fields of the same name from other messages in this package/namespace
using my_field = VariantField<
    MyMessage,
    std::variant<
      ::google::protobuf::BoolValue,
      ::google::protobuf::FloatValue,
      ::google::protobuf::Int32Value,
      ::google::protobuf::Int64Value,
    >
  >;
}
```


```
class Any {

}
```
