# V8 (kun's fork)

| | |
| --- | --- |
| Upstream | https://github.com/denoland/v8: V8 with Deno's patches, a branch per V8 version (`15.2-lkgr-denoland`), re-made by Deno's autoroll and force-pushed |
| Fork | https://github.com/wusyong/v8 |
| Branch | `kun`, from `c4ca1ecc` (V8 15.2.124, what rusty_v8's `v152.2.0` pins) |
| License | BSD-3-Clause (`LICENSE`) |
| Used by | kun's rusty_v8 fork (https://github.com/wusyong/rusty_v8, branch `kun`): its `v8` submodule, whose glue (`src/binding.cc`) and Rust (`src/kun.rs`) bind what this adds; kun's `bindings` builds on that |

## Why a fork

kun's DOM lives on kun's own GC (Rust, not cppgc), and its JS documents
need that heap marked with V8's major GCs, as Chrome marks Blink's cppgc
heap with V8's (the unified heap): V8 handing the embedder heap the objects
of the wrappers it marks, the embedder heap handing V8 the
`TracedReference`s its objects hold. V8's API for an embedder heap that
isn't cppgc (`EmbedderHeapTracer`) is gone; only `CppHeap` is left, and
its marking is cppgc's. So the hooks are in V8 itself, next to `CppHeap`'s.

## Local modifications

Code changes are marked with `kun:` comments.

- **An embedder heap marking with V8's major GCs** (2026-10-28):
  `include/v8-kun.h` (in a header of its own, so that changing it rebuilds
  only what includes it), and:
  - `src/heap/cppgc-js/cpp-heap.{h,cc}`: a `CppHeap` holds the embedder's
    `kun::EmbedderMarking`, started with a major GC's marking
    (`InitializeMarking`), advanced in each of V8's steps after cppgc's
    with what the step leaves (V8's main thread's handed objects published
    first: with `--single-threaded-gc` V8 shares none of that thread's
    work), asked whether it is done (`IsMarkingDone`), and ended in the
    pause (`FinishMarkingAndProcessWeakness`). Its used bytes count with
    cppgc's (`EmbedderSizeOfObjects`, in `heap.cc`) and may start or
    advance a GC as cppgc's allocations do (`ReportEmbedderAllocation`).
    `kun::Wrap` and `Unwrap` put the embedder's object's address in a
    wrapper's `CppHeapPointerTable` entry with a tag of
    `kun::kTagRange` (`0x4000..=0x4fff`). `kun::TracedMarker` is the
    embedder heap's threads' marking of `TracedReference`s into V8's
    worklists (as `ConcurrentUnifiedHeapMarkingVisitor`).
    `kun::FinishMarking` finishes a GC that runs, before the embedder heap
    is unset (as Chrome before it detaches a `CppHeap`).
  - `src/heap/cppgc-js/cpp-marking-state.h`: each marking state holds the
    embedder's local list of objects to hand over, published and deleted
    with it, empty only once that list is.
  - `src/heap/marking-visitor-inl.h`: marking a wrapper whose entry has one
    of the tags hands its object to the embedder heap, not cppgc.
  - `src/heap/heap-write-barrier.cc`, `young-generation-marking-visitor-inl.h`,
    `minor-mark-sweep.cc`, `cppgc-js/cpp-snapshot.cc`,
    `src/profiler/heap-profiler.cc`: an entry with one of the tags isn't a
    cppgc object: the barrier, the young generation, the heap snapshot and
    the detached-wrapper query leave it alone. An object wrapped while V8
    marks is marked already on the embedder's side (black, or in its
    snapshot), so the barrier hands nothing over.

  Only with pointer compression: the tags are the `CppHeapPointerTable`'s.
  Nothing changes until an embedder marking is set: cppgc's heap and
  wrappers behave as upstream's.

## Updating

When rusty_v8 moves to a new V8 version: rebase `kun` onto the commit
that rusty_v8's new tag pins (on `denoland/v8`'s branch for that version),
check each `kun:` change against the code it hooks (`CppHeap`'s marking,
`MarkingVisitorBase::VisitCppHeapPointer`, `CppHeapObjectWrapper`), push
it, and point kun's rusty_v8 fork's submodule at it (that fork's
`README.kun.md`, "Updating").
