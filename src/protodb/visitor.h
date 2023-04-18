#ifndef PROTODB_VISITOR_H__
#define PROTODB_VISITOR_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace google {
namespace protobuf {

struct WalkOptions {
  bool files = false;
  bool packages = false;  // not supported
  bool messages = false;
  bool enums = false;
  bool enum_values = false;
  bool fields = false;
  bool services = false;
  bool methods = false;
  bool extensions = false;

  static constexpr auto All() {
    return WalkOptions{
        .files = true,
        .packages = true,
        .messages = true,
        .enums = true,
        .enum_values = true,
        .fields = true,
        .services = true,
        .methods = true,
        .extensions = true,
    };
  }
};

// Visit every node in the descriptors calling `visitor(node)`.
template <typename VisitFunctor>
struct DescriptorVisitor {
  const WalkOptions& options;
  VisitFunctor visit_fn;

  auto indent() { return visit_fn.WithIndent(); }

  void Walk(const FieldDescriptor* descriptor) {
    if (descriptor->is_extension()) {
      if (options.extensions) {
        visit_fn(descriptor);
      }
    } else {
      if (options.fields) {
        visit_fn(descriptor);
      }
    }
  }

  void Walk(const EnumDescriptor* descriptor) {
    std::optional<decltype(indent())> with_indent;
    if (options.enums) {
      visit_fn(descriptor);
      with_indent.emplace(indent());
    }
    for (int i = 0; i < descriptor->value_count(); i++) {
      Walk(descriptor->value(i));
    }
  }

  void Walk(const EnumValueDescriptor* descriptor) {
    if (options.enum_values) {
      visit_fn(descriptor);
    }
  }

  void Walk(const ServiceDescriptor* descriptor) {
    std::optional<decltype(indent())> with_indent;
    if (options.services) {
      visit_fn(descriptor);
      with_indent.emplace(indent());
    }
    for (int i = 0; i < descriptor->method_count(); i++) {
      Walk(descriptor->method(i));
    }
  }

  void Walk(const MethodDescriptor* descriptor) {
    if (options.methods) {
      visit_fn(descriptor);
    }
  }

  void Walk(const Descriptor* descriptor) {
    std::optional<decltype(indent())> with_indent;
    if (options.messages) {
      visit_fn(descriptor);
      with_indent.emplace(indent());
    }
    for (int i = 0; i < descriptor->enum_type_count(); i++) {
      Walk(descriptor->enum_type(i));
    }
    for (int i = 0; i < descriptor->nested_type_count(); i++) {
      Walk(descriptor->nested_type(i));
    }
    for (int i = 0; i < descriptor->field_count(); i++) {
      Walk(descriptor->field(i));
    }
    for (int i = 0; i < descriptor->extension_count(); i++) {
      Walk(descriptor->extension(i));
    }
  }

  void Walk(const FileDescriptor* descriptor) {
    std::optional<decltype(indent())> with_indent;
    if (options.files) {
      visit_fn(descriptor);
      with_indent.emplace(indent());
    }
    for (int i = 0; i < descriptor->message_type_count(); i++) {
      Walk(descriptor->message_type(i));
    }
    for (int i = 0; i < descriptor->enum_type_count(); i++) {
      Walk(descriptor->enum_type(i));
    }
    for (int i = 0; i < descriptor->extension_count(); i++) {
      Walk(descriptor->extension(i));
    }
    for (int i = 0; i < descriptor->service_count(); i++) {
      Walk(descriptor->service(i));
    }
  }

  void Walk(const std::vector<const FileDescriptor*>& descriptors) {
    for (const FileDescriptor* descriptor : descriptors) {
      Walk(descriptor);
    }
  }
};

// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
template <typename VisitFunctor>
void WalkDescriptor(const WalkOptions& walk_options,
                    const FileDescriptor* descriptor, VisitFunctor visit_fn) {
  struct CompleteVisitFunctor : VisitFunctor {
    using VisitFunctor::operator();
    explicit CompleteVisitFunctor(VisitFunctor visit_fn)
        : VisitFunctor(visit_fn) {}
    void operator()(const void*) const {}
  };
  DescriptorVisitor<CompleteVisitFunctor>{
      .options = walk_options,
      .visit_fn = CompleteVisitFunctor(visit_fn),
  }
      .Walk(descriptor);
}

// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
template <typename VisitFunctor>
void WalkDescriptors(const WalkOptions& walk_options,
                     std::vector<const FileDescriptor*>& descriptors,
                     VisitFunctor visit_fn) {
  struct CompleteVisitFunctor : VisitFunctor {
    using VisitFunctor::operator();
    explicit CompleteVisitFunctor(VisitFunctor visit_fn)
        : VisitFunctor(visit_fn) {}
    void operator()(const void*) const {}
  };

  DescriptorVisitor<CompleteVisitFunctor>{
      .options = walk_options,
      .visit_fn = CompleteVisitor(visit_fn),
  }
      .Walk(descriptors);
}

}  // namespace protobuf
}  // namespace google

#endif