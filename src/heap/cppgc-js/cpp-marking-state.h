// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_

#include <memory>

#include "src/heap/cppgc-internal/marking-state.h"
#include "src/heap/cppgc-internal/marking-worklists.h"
#include "include/v8-machopo.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/objects/embedder-data-slot.h"

namespace v8 {
namespace internal {

class JSObject;
class EmbedderDataSlot;

class CppMarkingState final {
 public:
  explicit CppMarkingState(
      cppgc::internal::MarkingStateBase& main_thread_marking_state)
      : owned_marking_state_(nullptr),
        marking_state_(main_thread_marking_state) {}

  explicit CppMarkingState(std::unique_ptr<cppgc::internal::MarkingStateBase>
                               concurrent_marking_state)
      : owned_marking_state_(std::move(concurrent_marking_state)),
        marking_state_(*owned_marking_state_) {}
  CppMarkingState(const CppMarkingState&) = delete;
  CppMarkingState& operator=(const CppMarkingState&) = delete;

  // machopo: the embedder heap's local list of objects to hand over, with this state
  // (include/v8-machopo.h).
  ~CppMarkingState() {
    if (machopo_local_) machopo_marking_->DeleteLocal(machopo_local_);
  }
  void set_machopo_local(v8::machopo::EmbedderMarking* marking, void* local) {
    machopo_marking_ = marking;
    machopo_local_ = local;
  }
  V8_INLINE void HandObject(void* object) {
    if (machopo_local_) machopo_marking_->Hand(machopo_local_, object);
  }
  // The objects handed over on this thread, published for the
  // embedder heap's marking: the main thread's, before each of its steps
  // (V8 publishes that state's other work only when it shares it).
  void PublishHandedObjects() {
    if (machopo_local_) machopo_marking_->Publish(machopo_local_);
  }

  void Publish() {
    marking_state_.Publish();
    // machopo: the objects handed over too.
    if (machopo_local_) machopo_marking_->Publish(machopo_local_);
  }

  V8_INLINE void MarkAndPush(void* instance) {
    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(instance));
  }

  bool IsLocalEmpty() const {
    // machopo: and no object left to hand over.
    return marking_state_.marking_worklist().IsLocalEmpty() &&
           (!machopo_local_ || machopo_marking_->IsLocalEmpty(machopo_local_));
  }

 private:
  std::unique_ptr<cppgc::internal::MarkingStateBase> owned_marking_state_;
  cppgc::internal::MarkingStateBase& marking_state_;
  v8::machopo::EmbedderMarking* machopo_marking_ = nullptr;
  void* machopo_local_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
