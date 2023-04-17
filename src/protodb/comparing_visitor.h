#ifndef PROTODB_COMPARING_VISITOR_H__
#define PROTODB_COMPARING_VISITOR_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "absl/strings/string_view.h"
//#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
//#include "google/protobuf/descriptor_database.h"
#include "protodb/visitor.h"

namespace google {
namespace protobuf {

struct CompareOptions {
  bool files = false;
  bool packages = false; // not supported
  bool messages = false;
  bool enums = false;
  bool enum_values = false;
  bool fields = false;
  bool services = false;
  bool methods = false;

  static constexpr auto All() {
    return CompareOptions{
      .files = true,
      .packages = true,
      .messages = true,
      .fields = true,
      .enums = true,
      .enum_values = true,
      .services = true,
      .methods = true,
    };
  }
};


// Visit every node in the descriptors calling `visitor(node)`.
template <typename VisitFunctor>
struct ComparingDescriptorVisitor {
  const CompareOptions& options;
  VisitFunctor visit_fn;

  template<typename T, typename K = std::string_view>
  void CompareMap(std::map<K, T>& lhs, std::map<K, T>& rhs) {
    auto lhs_iter = lhs.begin();
    auto rhs_iter = rhs.begin();
    while (lhs_iter != lhs.end() && rhs_iter != rhs.end()) {
      if (lhs_iter == lhs.end()) {
        Compare(nullptr, rhs_iter->second); 
        rhs_iter++;
      } else if (rhs_iter == rhs.end()) {
        Compare(lhs_iter->second, nullptr); 
        lhs_iter++;
      } else {
        if (lhs_iter->first == rhs_iter->first) {
          Compare(lhs_iter->second, rhs_iter->second); 
          lhs_iter++;
          rhs_iter++;
        } else if (lhs_iter->first < rhs_iter->first) {
          Compare(lhs_iter->second, nullptr); 
          lhs_iter++;
        } else {
          Compare(nullptr, rhs_iter->second); 
          rhs_iter++;
        }
      }
    }
  }
  auto indent() { return visit_fn.WithIndent(); }

  void Compare(const FieldDescriptor* lhs, const FieldDescriptor* rhs) { 
    if (options.fields) {
      visit_fn(lhs, rhs); 
    }
  }

  void Compare(const EnumDescriptor* lhs, const EnumDescriptor* rhs) {
    std::optional<decltype(indent())> with_indent;
    if (options.enums) {
      //visit_fn(lhs->valu, rhs->descriptor); 
      //with_indent.emplace(indent());
    }
    // for (int i = 0; i < descriptor->value_count(); i++) {
    //   Compare(descriptor->value(i));
    // }
  }

  void Compare(const EnumValueDescriptor* lhs, const EnumValueDescriptor* rhs) {
    // if (options.enum_values) {
    //   visit_fn(descriptor); 
    // }
  }

  void Compare(const ServiceDescriptor* lhs, const ServiceDescriptor* rhs) {
    std::optional<decltype(indent())> with_indent;
    // if (options.services) {
    //   visit_fn(descriptor); 
    //   with_indent.emplace(indent());
    // }
    // for (int i = 0; i < descriptor->method_count(); i++) {
    //   Compare(descriptor->method(i));
    // }
  }

  void Compare(const MethodDescriptor* lhs, const MethodDescriptor* rhs) {
    // if (options.methods) {
    //   visit_fn(descriptor); 
    // }
  }

  void Compare(const Descriptor* lhs, const Descriptor* rhs) {
    std::optional<decltype(indent())> with_indent;
    if (options.messages) {
      visit_fn(lhs, rhs); 
      with_indent.emplace(indent());
    }
    if (options.enums) {
      std::map<std::string_view, const EnumDescriptor*> lhs_enums;
      std::map<std::string_view, const EnumDescriptor*> rhs_enums;
      for (int i = 0; i < lhs->enum_type_count(); i++) {
        lhs_enums[lhs->enum_type(i)->name()] = lhs->enum_type(i);
      }
      for (int j = 0; j < rhs->enum_type_count(); j++) {
        rhs_enums[rhs->enum_type(j)->name()] = rhs->enum_type(j);
      }
      CompareMap(lhs_enums, rhs_enums);
    }
    // for (int i = 0; i < lhs->nested_type_count(); i++) {
    //   Compare(lhs->nested_type(i));
    // }
    // for (int i = 0; i < lhs->field_count(); i++) {
    //   Compare(lhs->field(i));
    // }
    // for (int i = 0; i < lhs->extension_count(); i++) {
    //   Compare(lhs->extension(i));
    // }
  }

  void Compare(const FileDescriptor* lhs, const FileDescriptor* rhs) {
    std::optional<decltype(indent())> with_indent;
     if (options.files) {
       visit_fn(lhs, rhs); 
       with_indent.emplace(indent());
    }
    if (options.messages) {
      std::map<std::string_view, const Descriptor*> lhs_messages;
      std::map<std::string_view, const Descriptor*> rhs_messages;
      for (int i = 0; lhs && i < lhs->message_type_count(); i++) {
        lhs_messages[lhs->message_type(i)->name()] = lhs->message_type(i);
      }
      for (int j = 0; rhs && j < rhs->message_type_count(); j++) {
        rhs_messages[rhs->message_type(j)->name()] = rhs->message_type(j);
      }
      CompareMap(lhs_messages, rhs_messages);
    }
    if (options.enums) {
      std::map<std::string_view, const EnumDescriptor*> lhs_enums;
      std::map<std::string_view, const EnumDescriptor*> rhs_enums;
      for (int i = 0; lhs && i < lhs->enum_type_count(); i++) {
        lhs_enums[lhs->enum_type(i)->name()] = lhs->enum_type(i);
      }
      for (int j = 0; rhs && j < rhs->enum_type_count(); j++) {
        rhs_enums[rhs->enum_type(j)->name()] = rhs->enum_type(j);
      }
      CompareMap(lhs_enums, rhs_enums);
    }
    // for (int i = 0; i < descriptor->enum_type_count(); i++) {
    //   Compare(descriptor->enum_type(i));
    // }
    // for (int i = 0; i < descriptor->extension_count(); i++) {
    //   Compare(descriptor->extension(i));
    // }
    // for (int i = 0; i < descriptor->service_count(); i++) {
    //   Compare(descriptor->service(i));
    // }
  }

  void Compare(const std::vector<const FileDescriptor*>& lhs,
               const std::vector<const FileDescriptor*>& rhs) {
    std::map<std::string_view, const FileDescriptor*> lhs_files;
    std::map<std::string_view, const FileDescriptor*> rhs_files;
    for (const auto* file_descriptor : lhs) {
      lhs_files[file_descriptor->name()] = file_descriptor;
    }
    for (const auto* file_descriptor : rhs) {
      rhs_files[file_descriptor->name()] = file_descriptor;
    }
    CompareMap(lhs_files, rhs_files);
  }
};

// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
template <typename VisitFunctor>
void CompareDescriptor(
    const CompareOptions& walk_options,
    const FileDescriptor* lhs_descriptor,
    const FileDescriptor* rhs_descriptor,
    VisitFunctor visit_fn) {
  struct CompleteVisitFunctor : VisitFunctor {
    using VisitFunctor::operator();
    explicit CompleteVisitFunctor(VisitFunctor visit_fn) : VisitFunctor(visit_fn) {}
    void operator()(const void*) const {}
  };
  ComparingDescriptorVisitor<CompleteVisitFunctor>{
    .options = walk_options,
    .visit_fn = CompleteVisitFunctor(visit_fn),
   }.Compare(lhs_descriptor, rhs_descriptor);
}

// The visitor does not need to handle all possible node types. Types that are
// not visitable via `visitor` will be ignored.
template <typename VisitFunctor>
void CompareDescriptors(
    const CompareOptions& walk_options,
    const std::vector<const FileDescriptor*>& lhs_descriptors,
    const std::vector<const FileDescriptor*>& rhs_descriptors,
    VisitFunctor visit_fn) {
  struct CompleteVisitFunctor : VisitFunctor {
    using VisitFunctor::operator();
    explicit CompleteVisitFunctor(VisitFunctor visit_fn) : VisitFunctor(visit_fn) {}
    void operator()(const void*) const {}
  };

  ComparingDescriptorVisitor<CompleteVisitFunctor>{
    .options = walk_options,
    .visit_fn = CompleteVisitFunctor(visit_fn),
   }.Compare(lhs_descriptors, rhs_descriptors);
}

} // namespace google
} // namespace protobuf
 
#endif