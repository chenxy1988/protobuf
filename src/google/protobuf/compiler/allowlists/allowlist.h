// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_COMPILER_ALLOWLISTS_ALLOWLIST_H__
#define GOOGLE_PROTOBUF_COMPILER_ALLOWLISTS_ALLOWLIST_H__

#include <cstddef>
#include <cstring>

#include "absl/algorithm/container.h"
#include "google/protobuf/stubs/common.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace internal {
enum AllowlistFlags : unsigned int {
  kNone = 0,
  kMatchPrefix = 1 << 1,
  kAllowAllInOss = 1 << 2,
};

// An allowlist of things (messages, files, targets) that are allowed to violate
// some constraint.
//
// This is fundamentally a simple API over a set of static strings. It should
// only ever be used as a `static const` variable.
//
// These allowlists are usually only used internally within Google, and contain
// the names of internal files and Protobufs. In open source, these lists become
// no-ops (either they always or never allow everything).
template <size_t n>
class Allowlist final {
 public:
  template <size_t m = n, typename = std::enable_if_t<m != 0>>
  constexpr Allowlist(const absl::string_view (&list)[n], AllowlistFlags flags)
      : flags_(flags) {
    for (size_t i = 0; i < n; ++i) {
      list_[i] = list[i];
      if (i != 0) {
        ABSL_ASSERT(list_[i - 1] < list_[i] && "Allowlist must be sorted!");
      }
    }
  }

  template <size_t m = n, typename = std::enable_if_t<m == 0>>
  explicit constexpr Allowlist(AllowlistFlags flags)
      : list_(nullptr, 0), flags_(flags) {}

  // Checks if the element is allowed by this allowlist.
  bool Allows(absl::string_view name) const {
    if (flags_ & AllowlistFlags::kAllowAllInOss) return true;

    // Convert to a span to get access to standard algorithms without resorting
    // to horrible things like std::end().
    absl::Span<const absl::string_view> list = list_;

    auto bound = absl::c_lower_bound(list, name);
    if (bound == list.end()) {
      // If this string has the last element as a prefix, it will appear as if
      // the element is not present in the list; we can take care of this case
      // by manually checking the last element.
      //
      // This will also spuriously fire if a string sorts before everything in
      // the list, but in that case the check will still return false as
      // expected.
      if (flags_ & AllowlistFlags::kMatchPrefix && !list.empty()) {
        return absl::StartsWith(name, list.back());
      }

      return false;
    }

    if (name == *bound) return true;

    if (flags_ & AllowlistFlags::kMatchPrefix && bound != list.begin()) {
      return absl::StartsWith(name, bound[-1]);
    }

    return false;
  }

 private:
  constexpr absl::Span<const absl::string_view> list() const { return list_; }

  // NOTE: std::array::operator[] is *not* constexpr before C++17.
  //
  // In order for a zero-element list to work, we replace the array with a
  // null string view when the size is zero.
  std::conditional_t<n != 0, absl::string_view[n],
                     absl::Span<absl::string_view>>
      list_;
  AllowlistFlags flags_;
};

struct EmptyAllowlistSentinel {};

// This overload picks up MakeAllowlist({}), since zero-length arrays are not
// a thing in C++.
constexpr Allowlist<0> MakeAllowlist(
    EmptyAllowlistSentinel,  // This binds to `{}`.
    AllowlistFlags flags = AllowlistFlags::kNone) {
  return Allowlist<0>(flags);
}

template <size_t n>
constexpr Allowlist<n> MakeAllowlist(
    const absl::string_view (&list)[n],
    AllowlistFlags flags = AllowlistFlags::kNone) {
  return Allowlist<n>(list, flags);
}

}  // namespace internal
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_ALLOWLISTS_ALLOWLIST_H__
