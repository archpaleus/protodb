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
}

```


# Using a simplified Any proto
```
package protoid;
message Any {
  # We use higher tag numbers to make distinguishing the message easier.
  # The first four IDs are reserved and alwaysunused.  This makes it
  # easy to rule out any proto as a type matche if it's using
  # these field numbers, which is the majority of protos we'll see.
  reserved 1-4;

  # 64-bit ID of the message type.
  fixed64 id = 5;

  # Binary encoding data of the serialized message.
  bytes message = 6;

  # An optional checksum of the message using CRC-32.
  # Useful when persisting on disk.
  fixed32 crc32 = 75;

  # An optional typename for the message, eg "google.protobuf.Empty".  
  # Useful for debugging or when the ID will be unavailable.
  string type_name = 76;
}

protoid_any: {
  id: 4560029131573256278
  message: ...
}
```


Using an AnySet with a repeated field, we can easily write out a large number of 
Any objects, which can be easily concatenated together and read later.
```
message AnySet {
  repeated Any = 3;
}
```

