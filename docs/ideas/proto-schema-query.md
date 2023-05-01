
# Viewing FileDescriptorSet data
```
proto-schema-query dump all —- proto//**/*.proto
proto-schema-query dump files —- proto//**/*.proto
proto-schema-query dump messages —- proto//**/*.proto
proto-schema-query dump fields —- proto//**/*.proto      # Shows messages->fields
proto-schema-query dump enums —- proto//**/*.proto
proto-schema-query dump extensions —- proto//**/*.proto  # Shows all extensions per file/message
```

# Show dependencies
 - Show all deps for a given {file, message}
```
proto-schema-query deps my/file.proto —- proto//**/*.proto
proto-schema-query deps my.package.MyProto —- proto//**/*.proto
```


# Show graph of protos
 - Show graph for a given {file, message}
```
proto-schema-query graph files —- proto//**/*.proto       # Also include dependencies?
proto-schema-query graph messages —- proto//**/*.proto
```
