
# Viewing FileDescriptorSet data
```
descriptortool dump all —- proto//**/*.proto
descriptortool dump files —- proto//**/*.proto
descriptortool dump messages —- proto//**/*.proto
descriptortool dump fields —- proto//**/*.proto      # Shows messages->fields
descriptortool dump enums —- proto//**/*.proto
descriptortool dump extensions —- proto//**/*.proto  # Shows all extensions per file/message
```

# Show dependencies
 - Show all deps for a given {file, message}
```
descriptortool deps my/file.proto —- proto//**/*.proto
descriptortool deps my.package.MyProto —- proto//**/*.proto
descriptortool deps my.package.MyProto [ descriptor.pb proto//**/*.proto ]
```


# Show graph of protos
 - Show graph for a given {file, message}
```
descriptortool graph files —- proto//**/*.proto       # Also include dependencies?
descriptortool graph messages —- proto//**/*.proto
```
