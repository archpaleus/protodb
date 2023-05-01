# Protobuf Schema Database

Stores a copy of your existing database (ie FileDescriptorSet) in a local file
which is easily accessible.

Using the stored FileDescriptorSet you can then easily
 - detect breaking changes
 - share schema information
 - acess schema information for tools (eg Charles Proxy, printproto)
 - run tools to modify the school
 - generate code for any language


# Basic Usage

# Staging changes
When .proto files can be parsed and merged into the FileDescriptorSet
database with the `stage` command.  This can be run multiple times to
add new .proto files.  Files can be committed multiple times as long
as their definition is exactly the same.
```
proto-schema-db stage protobuf/src//google/protobuf/*.proto
proto-schema-db stage proto//**/*.proto protobuf/src//google/**/*.proto
```

## Committing Snapshots
After protos have been staged, a complete "snapshot" of the FileDescriptorSet
can be committed to history.
```
proto-schema-db snapshot filedescriptorset.binproto
```

### Snapshot version and labels
A snapchat can be labeled with a unique identifier such as a timestamp, 
git commit hash, CI build number, or a changeset/revision ID.  By default,
the current Unix timestamp in seconds will be used.  Snapshots will
be kept in the storage location with their specific label.  Depending on the
storage used, each label may create a separate copy of the data so be careful
creating too many labels. 
```
proto-schema-db snapshot:aa7e1cc6b proto//**/*.proto protobuf/src//google/protobuf/*.proto
```

# Staging changes
When .proto files have been modified a new FileDescriptorSet can be generated
for comparison against the existing snapshot.
```
proto-schema-db stage proto//**/*.proto protobuf/src//google/protobuf/*.proto
```


# Breaking change detection
While Schema DB can be used for breaking change detection, it was not built for
this purposed.  We recommend using `buf` for this.

# Schema database storage
Schema database files are stored locally in a directory called `.proto-schema-db/`.
Each file is accessible in the database via `/{label}.filedescriptorset.binproto`.
The label for a new snapshot will default to the current Unix timestamp, if none is provided.  On local
filesystems that support symlinking, these paths are symlinked to a backing
file so that multiple labels may refer to the same file without duplicating
storage data.  When storing on remote filesystems like GCS or S3, the snapshot
files are always duplicated.

# Configuration
Configuration is stored in `.proto-schema-db.conf`.  
The configuration is searched by looking in the current directory and all parent
directories until a file is found.  If no file is found, then the home directory
is searched.

# Restoring a snapshot
```
proto-schema-db stage proto//**/*.proto protobuf/src//google/protobuf/*.proto
```

# Fetching a remote snapshot
Snapshots can be fetch from a remote location via URL.  The label must always
be the last part of the URL without any suffix information.  Snapshots will 
be stored in the local database using the same label.
```
proto-schema-db fetch "gs://my-bucket/${label}"
proto-schema-db fetch "https://bucket.storage.com/snapshots/${label}"
```

```
proto-schema-db export saved-file-descriptor-set.pb
proto-schema-db export - >saved-file-descriptor-set.pb
```

# Accessing the current datasbase
You can always access the current snapshot via the symlink `.protodb'.
```
protoc --descriptor_set_in .protodb 
```
