# IDEAS FOR COMMAND LINE ARGS

### Flags

Traditionally command line flags are all indicated by flags with the exception
of a few.

`inspectproto` / `printproto`

```
printproto mydata.pb -i descriptor1.pb -i descriptor2.pb -- proto//**/*.proto
inspectproto mydata.pb -i descriptor.pb,descriptor2.pb -- proto//**/*.proto
```

```
# Flags work well in this case to provide semantic meaning for various
# parameters, effectively distinguishing inputs and outputs.
# Buf provides a great example of sane defaults.
compileproto -o mydesc.pb -i vendor.pb  proto//**/*.proto
```

```
descriptortool --descriptor descriptor.pb ls files google/protobuf/
descriptortool --descriptor descriptor.pb prune files proto//**/*.proto
descriptortool --descriptor - proto//**/*.proto  # No obvious output path.
descriptortool --descriptor /dev/null build proto//**/*.proto  # No obvious output path.
```

### Positional Arguments

This is the simplest and most restrictive case for users.  It provides limited ability to extend the command line to custom or novel use-cases, and in some cases provides less context to the uninitiated reader.   This may be appropriate for very simple command-line tools, but won't be sufficient for tools with more complex arguments.

```
# Not obvious what each positional argument means.
# Which field is input, which field is additional data?
printproto mydata.pb descriptor.pb proto//**/*.proto
inspectproto mydata.pb descriptor.pb proto//**/*.proto
```

```
# Imagine a compile command that generates a descriptor set and relies on
# reading in new .proto files as well as a previously compiled descriptor set.
# In this case, positional arguments provide no semantic meaning for each
# argument.
compileproto mydesc.proto other_desc.proto proto//**/*.proto
```

```
descriptortool descriptor.pb ls files google/protobuf/
descriptortool ! descriptor.pb proto//**/*.proto build
descriptortool - build proto//**/*.proto  # No obvious output path.
descriptortool /dev/null build proto//**/*.proto  # No obvious output path.
```

### Operators

As a shorthand notation we can use common operators such as `+`, `++`, `-`,
`--`, `,`, and `!` on the command-line.  These can be trickier for users and
may have some unexpected rough edges when they are used incorrectly, because
some operators are interpreted by the shell, and some can interfere with
command-line completion.

- `-`: indicate input/output should use stdin/stdout
- `!`: indicate next param is the input file that can be modified in-place.
- `-!`: use stdin/stdout and always write output
- `+`: next argument is supplemental data (eg descriptor set, .proto file), it will not be modified
- `+!`: next argument is supplemental data and mutable
- `++`: all following arguments are supplemental data until another operator is seen
- `--`: stop parsing any further arguments as operators/flags
- `[` / `]`: bracket multiple input .proto files
- `@`: read arguments from a file

# Use cases in practice

### Viewing a binary-encoded proto file with `inspectproto`/`printproto`

```
# The ++ is required when globbing.  This would be an easy mistake to make and confusing to fix.
printproto mydata.pb + descriptor.pb + google/protobuf/. ++ proto//**/*.proto

# Multiple files can be provided as single arg, separated by commas; ++ is still required for globbing.
printproto mydata.pb + descriptor.pb,google/protobuf/. ++ proto//**/*.proto

# This syntax is the easiest to use.  No further `+` is needed.
# It works fine for inspectproto/printproto, but doesn't allow descriptortool to
# place commands at the end
inspectproto mydata.pb ++ descriptor.pb google/protobuf/. proto//**/*.proto

# Dashes can be interpreted without being parsed as operators/flags when following `--`.
inspectproto mydata.pb ++ -- - -- -//**/*.proto

# Reading from stdin range from straightforward to perplexing
inspectproto ++ proto//**/*.proto < mydata.pb    # Implicilty read from stdin
inspectproto - ++ proto//**/*.proto < mydata.pb  # Explicitly read from stdin
inspectproto /dev/stdin ++ proto//**/*.proto < mydata.pb  # Read using dev filename
```

### View and manipulate a descriptor set.

```
# Basic usage is to provide the descriptor first followed by commands.
descriptortool descriptor.pb ls files google/protobuf/
descriptortool - ls files google/protobuf/ <descriptor.pb

# Some descriptors have dependencies that must be provided to properly validate
# the desciptor set.
descriptortool desc.pb + desc2.pb ls files ./proto/*.proto
descriptortool - + desc2.pb ls files google/protobuf/ <descriptor.pb >new-descriptor.pb

# If `++` is used to consume multiple arguments, then the rgument list must be
#  terminated by `--` before specifying the commands.
descriptortool - ++ *.pb -- ls files google/protobuf/

# To modify the descriptor set, you must pass `!` as the first argument.
# This is not required when reading from stdin because the input is not modified.
descriptortool ! descriptor.pb drop files google/protobuf/ ./proto/*.proto
descriptortool - drop files google/protobuf/ <descriptor.pb >new-descriptor.pb

# OPTIONAL: If stdout is detected to be a TTY, binary descriptor set data will not be written.
# Using `!` in front of `-` will force output to stdout, even if it is a TTY.
descriptortool ! - + desc2.pb drop files google/protobuf/ <descriptor.pb >new-descriptor.pb

# OPTIONAL: we can support modifying multiple descriptor sets at a time.
descriptortool ! desc1.pb ! desc2.pb drop files google/protobuf/ ./proto/*.proto

# OPTIONAL: If all descriptor set's are prefixed with a `+` then we can instead
# use `+!` to prefix for modified files.   This could also apply to `-`
# however `-!` begins to look like a flag.
descriptortool +! desc1.pb +! desc2.pb drop files google/protobuf/
descriptortool +! desc1.pb,desc2.pb drop files google/protobuf/
descriptortool -! desc1.pb + desc2.pb drop files google/protobuf/

# OPTIONAL: If all descriptor set's are prefixed with a `+` then we can instead
#  use `+!` to prefix for modified files.
descriptortool ++! desc1.pb  desc2.pb drop files google/protobuf/ ./proto/*.proto

# The following syntax doesn't work well because there is a lack of semantic meaning.
descriptortool ! desc.pb ++ src//google/protobuf/*.proto drop files ./proto/*.proto

# OPTIONAL: Use `[` / `]` to bracket .proto files that should be compiled
descriptortool +! desc1.pb [ proto/**/.proto ] drop files google/protobuf/
```

Manage a local proto descriptor database

```
protodb init proto//**/*.proto
protodb = proto//**/*.proto
protodb add proto//**/*.proto
protodb ++ proto//**/*.proto
protodb remove
```

### Accidental User Errors

Typing the ! without a space will trigger shell history completion.  The
resulting command will depend on the user's shell history, which can lead
to some very unexpected behaviors.

```
inspectproto !myproto.db  # UNEXPECTED!
```

### Available command line options

There is a dizzying number of custom shell parsing and expansion options for users to understand.  Additionally, zsh has many more options for pattern matching compared to bash.

Valid unintepreted command line parameters
```
echo $ a$
echo [ ] ]]
echo ^ ^^ ^a ^1 ^- ^+ ^@ ^= ^^ ^% ^$ ^!
echo @ @@ @a @1 @- @+ @@ @= @^ @% @$ @!
echo % %% %a %1 %- %+ %@ %= %^ %% %$ %!
echo + ++ +a +1 +- ++ +@ += +^ +% +$ +!
echo - -- -a -1 -- -+ -@ -= -^ -% -$ -!
echo : :: :a :1 :- :+ :@ := :^ :% :$ :!
```
It can be tricky and dangerous to use operators that are commonly used in shell
expansions, such as ! and $.  While they are safe on their own, a simple typo that
causes them to merge with anohter character can dramatically change the meaning of
command executed by the shell.

Interpreted command line parameters
```
# Any letter following a [ is considered part of an escape sequence. Brackets
# are interpreted within a string unless escaped
echo [a [[ [! [@ a[

# Dollar sign precedes environment variable expansion, even when within a string.
# All of the following are interpreted by the shell.
echo $$ $1 a$1 a$@

Anything following an equal sign is interpreted as a unmodified file.
echo = == =a =1 =- =+ =@ == =^ =%
```


# Final thoughts

- descriptortool should only work on descriptor sets -- no .proto files; mixing cases is a bit too confusing
