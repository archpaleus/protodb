#ifndef PROTODB_VISITOR_H__
#define PROTODB_VISITOR_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "absl/container/btree_map.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/port.h"
#include "google/protobuf/protodb/actions.h"
#include "google/protobuf/repeated_field.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {

template <typename Visitor>
struct VisitImpl {
  Visitor visitor;
  void Visit(const FieldDescriptor* descriptor) { visitor(descriptor); }

  void Visit(const EnumDescriptor* descriptor) { visitor(descriptor); }

  void Visit(const Descriptor* descriptor) {
    visitor(descriptor);

    for (int i = 0; i < descriptor->enum_type_count(); i++) {
      Visit(descriptor->enum_type(i));
    }

    for (int i = 0; i < descriptor->field_count(); i++) {
      Visit(descriptor->field(i));
    }

    for (int i = 0; i < descriptor->nested_type_count(); i++) {
      Visit(descriptor->nested_type(i));
    }

    for (int i = 0; i < descriptor->extension_count(); i++) {
      Visit(descriptor->extension(i));
    }
  }

  void Visit(const std::vector<const FileDescriptor*>& descriptors) {
    for (auto* descriptor : descriptors) {
      visitor(descriptor);
      for (int i = 0; i < descriptor->message_type_count(); i++) {
        Visit(descriptor->message_type(i));
      }
      for (int i = 0; i < descriptor->enum_type_count(); i++) {
        Visit(descriptor->enum_type(i));
      }
      for (int i = 0; i < descriptor->extension_count(); i++) {
        Visit(descriptor->extension(i));
      }
    }
  }
};

// Visit every node in the descriptors calling `visitor(node)`.
// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
// Disclaimer: this is not fully implemented yet to visit _every_ node.
// VisitImpl might need to be updated where needs arise.
template <typename Visitor>
void VisitDescriptors(const std::vector<const FileDescriptor*>& descriptors,
                      Visitor visitor) {
  // Provide a fallback to ignore all the nodes that are not interesting to the
  // input visitor.
  struct VisitorImpl : Visitor {
    explicit VisitorImpl(Visitor visitor) : Visitor(visitor) {}
    using Visitor::operator();
    // Honeypot to ignore all inputs that Visitor does not take.
    void operator()(const void*) const {}
  };

  VisitImpl<VisitorImpl>{VisitorImpl(visitor)}.Visit(descriptors);
}

} // namespace google
} // namespace protobuf
 
#endif