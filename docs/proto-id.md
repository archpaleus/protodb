# Proto ID

Provides a unique ID for all messages within a .proto closure that enables simpler
ways to identfy the format of a protobuf messages across the wrire.

Using these IDs, you can more succiently communicate your message type in an Any
message.   Writing a 64-bit protoid requires 5 bytes of data on the wire, keeping 
overhead minimal.  Compared to a google.protobuf.Any message, which is typically
more than 40 bytes to specify the message type.

In total an Any message has an overhead of 6-11 bytes on top of
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
