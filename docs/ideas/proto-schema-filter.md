# Protobuf Schema Filter

## Schema Annotations

```
option (filter.include_file_in) = { [protobunny.filter.ext.build]: SERVER};

message ServerOnlyMessage {
  option (filter.include_in) = { [protobunny.filter.ext.build]: SERVER};

  int32 go_only_field = 1; [
    option (filter.include_in) = { [protobunny.filter.ext.lang]: GO};
  ]
}

enum ServerOnlyEnum {
  option (filter.include_enum_in) = { [protobunny.filter.ext.build]: CLIENT };

}

enum Service {
  option (filter.include_service_in) = {
     [protobunny.filter.ext.build]: SERVER
     [protobunny.filter.ext.lang]: GO
     [protobunny.filter.ext.lang]: TYPESCRIPT
  };
  option (filter.include_service_in) = {
     [protobunny.filter.ext.build]: CLIENT
     [protobunny.filter.ext.lang]: KOTLIN
  };

  rpc Method(Request) returns (Response) [
    (filter.include_method_in) = { [protobunny.filter.ext.build]: SERVER }
  ]
}
```

## Modifying a FileDescriptorSet

### 1. Comamnd line tool

```
proto-schema-filter build=client <filedescriptorset.binproto >client-filedescriptorset.binproto
proto-schema-filter build=server <filedescriptorset.binproto >server-filedescriptorset.binproto
```

### 2. Running as a plugin
```
protoc --plugin=protoc-gen-schema-filter --schema_filter_opt="build=client" \
    --descriptor_set_out client-filedescriptorset.binproto  **/*.proto
protoc --plugin=protoc-gen-schema-filter --schema_filter_opt="build=server" \
    --descriptor_set_out server-filedescriptorset.binproto **/*.proto
```


# Existing Projects

- proto-filter
  - https://github.com/wdullaer/proto-filter