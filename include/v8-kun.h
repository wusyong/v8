// kun: an embedder heap marking with V8's major GCs as cppgc's does in the
// unified heap: kun's new DOM, on kun's own GC (crates/gc), with V8. In a
// header of its own, so that changing it rebuilds only what includes it.
//
// The embedder heap's objects are wrapped by their address, as cppgc's,
// with a tag of kTagRange (one per type): V8's marking hands the object
// to the heap's marking, and its write barrier, heap snapshot and young
// generation leave it alone. The objects never move. The embedder heap's
// marker hands V8 the TracedReferences its objects hold, from its own
// threads.

#ifndef INCLUDE_V8_KUN_H_
#define INCLUDE_V8_KUN_H_

#include <stddef.h>
#include <stdint.h>

#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-sandbox.h"       // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class CppHeap;
class Isolate;
class Object;
class TracedReferenceBase;

namespace kun {

/** The tags of wrappers whose CppHeapPointerTable entry holds an embedder
 * heap's object (not a cppgc one). */
constexpr CppHeapPointerTag kFirstTag = static_cast<CppHeapPointerTag>(0x4000);
constexpr CppHeapPointerTag kLastTag = static_cast<CppHeapPointerTag>(0x4fff);
constexpr CppHeapPointerTagRange kTagRange(kFirstTag, kLastTag);

/**
 * The embedder heap, marking with V8's major GCs: V8 starts its marking
 * with its own, advances it with each of its steps, asks whether it is done
 * before it finishes (in the pause, advancing it again while either side
 * finds more), and ends it once both are done.
 */
class EmbedderMarking {
 public:
  virtual ~EmbedderMarking() = default;

  /** A major GC's marking starts. */
  virtual void Start() = 0;

  /**
   * Marks for up to `max_duration_ms`, what V8's step leaves after cppgc's
   * (all that is left in the atomic pause). Returns whether nothing is
   * left.
   */
  virtual bool Advance(double max_duration_ms, bool in_atomic_pause) = 0;

  /** Whether nothing is left now (V8's locals published). */
  virtual bool IsDone() = 0;

  /**
   * Both sides are done (in the pause, after V8 cleared what died).
   * Returns the embedder heap's used bytes after the marking: its live
   * ones (what it marked, and what it allocated while it marked), not the
   * dead it has yet to sweep. V8 counts them as its size from now on and
   * sets its next limits from them, as from cppgc's marked bytes
   * (StatsCollector::NotifyMarkingCompleted).
   */
  virtual size_t End() = 0;

  /**
   * A marking thread's list of wrapped objects to hand over, with the
   * thread's marking state; published with it, and deleted with it.
   */
  virtual void* NewLocal() = 0;
  virtual void Hand(void* local, void* object) = 0;
  virtual void Publish(void* local) = 0;
  virtual bool IsLocalEmpty(void* local) = 0;
  virtual void DeleteLocal(void* local) = 0;
};

/** Sets (or, with null, clears) `heap`'s embedder heap. */
V8_EXPORT void SetEmbedderMarking(CppHeap* heap, EmbedderMarking* marking);

/**
 * The embedder heap's used bytes are `used` now (cppgc's
 * ReportBufferedAllocationSizeIfPossible, as a total): V8 counts them with
 * cppgc's, and on growth may start, advance or finish a major GC now. Only
 * between the embedder heap's operations.
 */
V8_EXPORT void ReportEmbedderUsedSize(Isolate* isolate, size_t used);

/** Finishes a major GC's marking that runs, atomically (its concurrent
 * markers joined): afterwards no V8 thread marks, nor calls the embedder
 * marking, until the next GC starts. Before the embedder heap is unset
 * and freed (Chrome finishes its GC before it detaches a CppHeap). */
V8_EXPORT void FinishMarking(Isolate* isolate);

/** Wraps `wrapper` (an API wrapper wrapping nothing) with the embedder
 * heap's `object` (not null), tagged `tag` (of kTagRange). */
V8_EXPORT void Wrap(Isolate* isolate, Local<Object> wrapper, void* object,
                    uint16_t tag);

/** The object `wrapper` holds with `tag`, null if none (another tag's, or
 * not an API wrapper: checked here, so callers need no IsApiWrapper). */
V8_EXPORT void* Unwrap(Isolate* isolate, Local<Object> wrapper, uint16_t tag);

/**
 * A thread's marking of the TracedReferences the embedder heap's objects
 * hold into V8's worklists (cppgc's ConcurrentUnifiedHeapMarkingVisitor),
 * on any thread, while V8 runs a major GC's marking; published when
 * deleted.
 */
class V8_EXPORT TracedMarker {
 public:
  explicit TracedMarker(Isolate* isolate);
  ~TracedMarker();
  TracedMarker(const TracedMarker&) = delete;
  TracedMarker& operator=(const TracedMarker&) = delete;

  void Mark(const TracedReferenceBase& reference);
  void Publish();

 private:
  void* impl_;
};

}  // namespace kun
}  // namespace v8

#endif  // INCLUDE_V8_KUN_H_
