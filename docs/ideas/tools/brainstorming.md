
Thesis:

A precompiled FileDescriptorSet guarantees compilation, 
but is less convenient when using protoc.  We make working
with a FileDescriptorset easier and more powerful.

```
# Read in descriptor sets from .proto schema, binary data from stdin
pb guess proto//**/*.proto <proto.pb

# Read in descriptor sets from .proto schema, binary data from file
#  NOTE: Using -f for --file clashes with -f for —force
pb guess -f proto.pb proto//**/*.proto

# Read in descriptor sets from a file, with backward compatibility to protoc
pb guess -i descriptor_set.pb  <proto.pb
pb guess --descriptor_set_in=descriptor_set.pb  <proto.pb
```

# Viewing FileDescriptorSet data
```
pb dump all —- proto//**/*.proto
pb dump files —- proto//**/*.proto       # Also include dependencies?
pb dump messages —- proto//**/*.proto
pb dump fields —- proto//**/*.proto      # Shows messages->fields
pb dump enums —- proto//**/*.proto
pb dump extensions —- proto//**/*.proto  # Shows all extensions per file/message
```

# Show graph of protos
 - Show all deps for a given {file, message}
```
pb graph all —- proto//**/*.proto
pb graph files —- proto//**/*.proto       # Also include dependencies?
pb graph messages —- proto//**/*.proto
pb graph fields —- proto//**/*.proto      # Shows messages->fields
pb graph enums —- proto//**/*.proto
pb graph extensions —- proto//**/*.proto  # Shows all extensions per file/message
```

# Create a new proto database
Database can be constructed from .proto files or existing descriptor sets.
Any existing database may be archived and restored later
Files are added in the order provided on the command line.
If a file is duplicated, then subsets are valid, but no existing schema can be changed.
```
# Generate a clean snapshot from .proto files.
pb snapshot proto//**/*.proto -i descriptorA.pb --descriptor_set_in=descriptorB.pb,descriptorC.pb
```

# Importing FileDescriptorSet data into the database
```
# Import a descriptor set into the database
pb import descriptor_set.pb 
pb import -i=descriptor_set.pb 
```

# Importing .proto schema into the database
```
# Add to a database.  Will error if protos are duplicated with modifications.
pb add —- proto//**/*.proto
# Use —-force to overwrite existing ports that are modified
pb add -f —- proto//**/*.proto

protodb add -- proto/**/*.proto
protodb add proto//**/*.proto protobuf/src//
 - equiv to -Iproto -Iprotobuf/src proto/**/*.proto
   
protodb add -i bazel-bin/proto/all-descriptor-set.proto.bin
protodb add bazel-bin/google/protobuf/all-descriptor-set.proto.bin
```

# Updating .proto schema in the database
```
# Update existing descriptors in database.  
# Will update any existing protos as long as there are no breaking changes.
# Breaking change Rules:
#   Removal of fields, must be replaced with a “reserved” keyword.
#   Default values can’t be changed
#
#   Cannot move from
#   Warn on option changes.
pb update —- proto//**/*.proto

# Use —-force to allow breaking changes.
pb update -f —- proto//**/*.proto

# Use —-dryrun to validate breaking changes, without updating local db.
pb update -d —- proto//**/*.proto
```

# Exporting database schema to a new file
```
# Export a descriptor set from existing database
pb export descriptor_set.pb
pb export - >descriptor_set.pb

# Export a descriptor set from existing database along with parsed .proto schema
pb export descriptor_set.pb —- proto//**/*.proto
pb export -o descriptor_set.pb —- proto//**/*.proto
```

# Print deps 
```
pb deps google.protobuf.DescriptorSet — proto//**/*.proto $(ls src//google/protobuf/**/*.proto | grep -v ruby)
```
 

# Initialize a new .protodb/ database.
```
protodb init
protodb init ~/
```


# Guess
 - guesses the type from available types in the database
 - options to print all valid matches
```
protodb guess <proto.bin
```

# Print a text formatted proto
 - will decode a raw proto into a message and print as text-formatted
 - type will be guessed and overlayed if a match is found
 - 'google.protobuf.Empty' or an equiv will be used if no type is available
``` 
# Decode binary data from stdin, guessing type from database
protodb decode <proto.bin

# Decode binary data from stdin, specify an unqualified type
protodb decode FileDescriptorSet <proto.bin

# Decode binary data from stdin, specify a qualified type
protodb decode google.protobuf.FileDescriptorSet <proto.bin
```



```
> printproto bazel-bin/proto/all-descriptor-set.proto.bin --hints=auto
1 {							               	(Message: FileDescriptor)
  1: "frobulator/widget.proto”	(23 bytes)
  2: “frobulator"               (10 bytes)
  4 {                           (Message: DescriptorProto) 
    1: “Widget”                 (6 bytes)
    2 {
      1: “name"                 (4 bytes)
      3: 1                      (1 byte)
      4: -1                     (5 bytes)
      5: 9
      10: "name"
    }
  }
  12: "proto3"
}
```

```
#printproto FileDescriptorSet all-descriptor-set.proto.bin
>cat all-descriptor-set.proto.bin | printproto FileDescriptorSet

file {									(FileDescriptor: 77 bytes)
  name: "frobulator/widget.proto”		(bytes: 1+23 bytes)
  package: "frobulator”					(bytes: 1+10 bytes)
  message_type {						(DescriptorProto: 3+24 bytes)
    name: "Widget"						(bytes: 1+6 bytes)
    field {                             (FieldDescriptorProto: 3+15 bytes)
      name: “name"                      (bytes: 1+4 bytes)
      number: 1
      label: LABEL_OPTIONAL
      type: TYPE_STRING
      json_name: "name"
    }
  }
  syntax: "proto3"
}
```

# printproto should try to guess if no type is supplied
```
>cat all-descriptor-set.proto.bin | printproto 

file {									(FileDescriptor: 77 bytes)
  name: "frobulator/widget.proto”		(bytes: 1+23 bytes)
  package: "frobulator”					(bytes: 1+10 bytes)
  message_type {						(DescriptorProto: 3+24 bytes)
    name: "Widget"						(bytes: 1+6 bytes)
    field {                             (FieldDescriptorProto: 3+15 bytes)
      name: “name"                      (bytes: 1+4 bytes)
      number: 1
      label: LABEL_OPTIONAL
      type: TYPE_STRING
      json_name: "name"
    }
  }
  syntax: "proto3"
}
```

```
> printproto bazel-bin/proto/all-descriptor-set-corrupted.proto.bin

1 {								(Message: FileDescriptor)
  1: "frobulator/widget.proto”	(23 bytes)
  2: “frobulator"               (10 bytes)
  4 {                           (Message: DescriptorProto) 
    1: “Widget”                 (6 bytes)
    2 {
      1: “name"                 (4 bytes)
Error: CORRUPTED TAG INPUT: 4F 71 D8
```

https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/descriptor.proto
