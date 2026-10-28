// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_

#include <memory>

#include "src/heap/cppgc-internal/marking-state.h"
#include "src/heap/cppgc-internal/marking-worklists.h"
#include "include/v8-kun.h"
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

  // kun: the embedder heap's local list of objects to hand over, with this state
  // (include/v8-kun.h).
  ~CppMarkingState() {
    if (kun_local_) kun_marking_->DeleteLocal(kun_local_);
  }
  void set_kun_local(v8::kun::EmbedderMarking* marking, void* local) {
    kun_marking_ = marking;
    kun_local_ = local;
  }
  V8_INLINE void HandObject(void* object) {
    if (kun_local_) kun_marking_->Hand(kun_local_, object);
  }
  // The objects handed over on this thread, published for the
  // embedder heap's marking: the main thread's, before each of its steps
  // (V8 publishes that state's other work only when it shares it).
  void PublishHandedObjects() {
    if (kun_local_) kun_marking_->Publish(kun_local_);
  }

  void Publish() {
    marking_state_.Publish();
    // kun: the objects handed over too.
    if (kun_local_) kun_marking_->Publish(kun_local_);
  }

  V8_INLINE void MarkAndPush(void* instance) {
    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(instance));
  }

  bool IsLocalEmpty() const {
    // kun: and no object left to hand over.
    return marking_state_.marking_worklist().IsLocalEmpty() &&
           (!kun_local_ || kun_marking_->IsLocalEmpty(kun_local_));
  }

 private:
  std::unique_ptr<cppgc::internal::MarkingStateBase> owned_marking_state_;
  cppgc::internal::MarkingStateBase& marking_state_;
  v8::kun::EmbedderMarking* kun_marking_ = nullptr;
  void* kun_local_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
