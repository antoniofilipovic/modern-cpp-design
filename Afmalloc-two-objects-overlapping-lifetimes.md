# AfMalloc: Does borrowing the next chunk's `prev_size` create two objects with overlapping lifetimes?

**Question.** `getMallocNeededSize` (src/allocators/AfMalloc.cpp:109) computes
`(size + SIZE_OF_SIZE + ALIGNMENT_MASK) & ~ALIGNMENT_MASK`, deliberately letting the user's
data region extend into the *next* chunk's `previous_size_` field (glibc boundary-tag design).
Does that violate the rule that two objects with overlapping lifetimes must occupy disjoint
bytes of storage — i.e., is the whole scheme undefined behavior?

**Verdict (short).** No. The "disjoint bytes" rule ([intro.object]/9) is an *invariant the
standard maintains*, not a precondition code can breach: creating an object in occupied storage
*ends* the previous occupant's lifetime at that same moment ([basic.life]/1.5), so ownership of
the borrowed 8 bytes is handed off over time and two live objects never coexist there. The one
genuinely under-specified corner — whether partially overwriting a `Chunk` kills the whole
`Chunk` or only the clobbered subobject — is resolved in practice (and by committee intent) in
favor of *subobject-only death*. The right hygiene tool for this code is `std::launder` on
pointers minted by address arithmetic; `std::start_lifetime_as` is only appropriate on memory
windows where nothing else is alive (e.g., top-chunk formation), and must **not** be used on an
allocated neighbor.

---

## 1. The design under scrutiny

An allocated chunk of `needed_size` bytes at address `c` hands the user the region
`[c+16, c+16+size)`. Because `needed_size = align16(size + 8)`, the user region can overshoot
`c + needed_size` (where the next chunk begins) by **at most 8 bytes** — exactly the next
chunk's `previous_size_` field `[next, next+8)`. The ASCII diagram in
`getMallocNeededSize` (AfMalloc.cpp:67–104) documents this: region *e)* — the neighbor's
`prev_size` — is usable space while our chunk is allocated, because that field is only
meaningful when *our* chunk is free.

Three byte regions are therefore time-shared between "allocator metadata" and "user data":

| Bytes (relative to chunk `c`)  | As metadata            | As user data                    |
|--------------------------------|------------------------|---------------------------------|
| `[c+16, c+32)`                 | our `prev_` / `next_`  | head of the user's region       |
| `[next, next+8)`               | neighbor's `previous_size_` | tail of the user's region (the borrow) |
| `[c, c+16)`                    | `previous_size_`, `size_` + flag | **never** — always metadata |

The last row is load-bearing: the fields the allocator *reads while the neighbor is allocated*
(`size_` and the `PREV_FREE` bit, at `[next+8, next+16)`) are never borrowable, because the
overshoot is capped at 8 bytes by the `+ SIZE_OF_SIZE` term.

## 2. Why "two live overlapping objects" cannot occur

The rule being worried about, [intro.object]/9:

> "Two objects with overlapping lifetimes that are not bit-fields may have the same address if
> one is nested within the other, …; otherwise, they have distinct addresses and occupy
> disjoint bytes of storage."

The mechanism that *maintains* this invariant, [basic.life]/1 bullet (1.5):

> "The lifetime of an object *o* of type T ends when: … **the storage which the object occupies
> is released, or is reused by an object that is not nested within *o***."

There is no operation in C++ that produces two live non-nested objects in the same byte.
Creating an object in occupied storage and the death of the old occupant are *one event*,
ordered old-ends-then-new-begins. Overlap in space over time is a handoff; overlap in space at
one instant is unrepresentable.

Timeline of the borrowed bytes **B = `[next, next+8)`** through one cycle:

1. **Chunk formed at `next`** (top chunk at AfMalloc.cpp:760–761, or a binned chunk's header).
   A `Chunk` is alive there; its `previous_size_` subobject occupies B. One owner — the
   subobject, *nested within* the `Chunk`, which is the explicitly permitted same-address case.
2. **User writes their data into B.** Storage reuse. Per [basic.life]/(1.5),
   `previous_size_`'s lifetime **ends at that instant** — the user's object is not nested
   within it. Sequential handoff, zero overlap. The allocator never reads `previous_size_`
   during the user's tenure: the reads (AfMalloc.cpp:310, 316) are gated by `isPrevFree()`
   (line 309), whose bit lives in `size_` — un-borrowable bytes.
3. **`free()` reclaims B**: `next_chunk->setPrevSize(free_chunk->getSize())`
   (AfMalloc.cpp:363, or 360 on the merge path) stores a `size_t` into B — another reuse event
   over the now-dead user data — then `setPrevFree()` raises the flag. Only after this write
   does any read of B ever occur (in a later `free` of the neighbor).

Every read of B is dominated by a write of B under the same ownership; every ownership change
is a reuse event that *by rule* terminates the prior lifetime. ∎

## 3. Partial reuse: does the whole `Chunk` die, or only `previous_size_`?

This is the one place the standard's text genuinely under-determines the answer. When the
user's data reuses B, [basic.life]/(1.5) says an object dies when "the storage which the object
occupies … is reused." The user's write reuses **all** of `previous_size_`'s storage but only
**8 of the `Chunk`'s 32 bytes**. Two readings:

- **Narrow (subobject-only death):** an object dies when *its* storage — the region identified
  with it — is reused. `previous_size_` dies; the containing `Chunk` survives with one dead
  subobject.
- **Broad (containment death):** reusing *any* byte of an object's region kills it, so the
  `Chunk` dies too — and every later `next_chunk->isPrevFree()` / `setSize()` would be member
  access on an out-of-lifetime object ([basic.life]/7), i.e., UB.

**The narrow reading is the correct one to rely on.** Support:

**(a) Textual.** The parallel bullet in the same sentence — "the storage … is *released*" —
is inherently a whole-region event (you deallocate an allocation, not a byte of it); reading
"reused" at the same granularity is the consistent interpretation. Moreover, P0593 amended this
bullet to add "by an object that is *not nested within* o" precisely so that objects created
*inside* an object (in storage it provides) don't kill it — the committee was tracking the
containment relation carefully here, at object granularity, not byte granularity.

**(b) Reductio: the broad reading breaks the standard's own blessed patterns.** Destroying a
member and placement-new'ing a replacement —

```cpp
struct C { T t; };
C c;
c.t.~T();
new (&c.t) T(args);   // reuses part of c's storage
c.t.use();            // universally accepted as fine
```

— reuses part of `c`'s storage with an object not nested within `c` (a placement-new'd `T` is a
*complete object*, not a subobject, and only `unsigned char`/`std::byte` arrays "provide
storage" per [intro.object]/3). Under the broad reading, `c` dies at the placement-new and
every subsequent use of `c` is UB. The transparent-replacement machinery of [basic.life]/8
exists precisely so that names and pointers keep working across such member replacement — it
would be purposeless if the enclosing object were killed by the very act it regulates.

**(c) Union precedent.** [class.union]/5 blesses a plain assignment as *beginning the lifetime*
of a union member. The committee has already accepted that raw stores can (re)establish
subobjects in place without ceremonies and without harming the enclosing object; the non-union
member case is a wording gap, not a different intent.

**(d) C parity — the reason glibc itself is fine.** In C (C11 6.5p6), a store to allocated
storage *changes the effective type* of the stored-to bytes and nothing else: overwriting the
neighbor's `prev_size` bytes with user data is natively defined, per-byte-range, with no notion
of "the containing struct dies." glibc malloc is compiled as C and lives entirely on this rule.
P0593's stated purpose is C parity for exactly this kind of code (its motivating examples are
malloc-style usage); a reading of C++ under which the identical bytes-and-stores are UB while
defined in C defeats the paper's point.

**(e) No implementation channel.** For the broad reading to bite, a compiler would need to
track byte-granular death of enclosing heap objects across arbitrary stores. No compiler has
such a mechanism: in GCC/LLVM IR, heap objects carry no lifetime metadata at all (lifetime
intrinsics exist only for stack slots), and the actual enforcement mechanism — TBAA — reasons
per access: the user's store touches `[next, next+8)`, the allocator's `size_t` loads touch
`[next+8, next+16)`; disjoint bytes, no aliasing assumption violated. The broad reading is not
just unimplemented — implementing it would break every allocator ever written, including the
compiler's own runtime.

**(f) Honest caveat.** This corner is acknowledged-defective standardese under active repair:
CWG 2676 ("Replacing a complete object having base subobjects") documents adjacent cases where
the subobject-replacement rules of [basic.life] visibly fail to say what everyone agrees they
should. Treat the narrow reading as the intended and de-facto semantics, not as settled clause
text.

## 4. Why **not** `std::start_lifetime_as` on the allocated neighbor

`start_lifetime_as<T>(p)` *creates* a `T` — it is a **reuse event over all of
`sizeof(T)` bytes**. That gives it blast radius the situation cannot afford:

- `start_lifetime_as<Chunk>(next)` in `free()` (where `next` may be an *allocated* neighbor)
  would formally reuse `[next, next+32)` — which includes `[next+16, next+32)`, the live head
  of the **neighbor user's data** (their region starts at `next+16`). You would formally end
  the lifetime of another user's live object to bless your own bookkeeping. Strictly worse
  than the murk it was meant to fix.
- The per-field version, `start_lifetime_as<std::size_t>(&next->previous_size_)`, doesn't
  compose either: it creates a **complete** `size_t` object, and [basic.life]/8's
  transparent-replacement conditions require *"either o1 and o2 are both complete objects, or
  o1 and o2 are direct subobjects of objects p1 and p2 … [with] p1 transparently replaceable
  by p2."* A complete object cannot transparently replace a *member* subobject, so the member
  access `next->previous_size_` afterwards still designates the dead member, not the newly
  created object. You'd have to route every access through the returned `size_t*`, at which
  point the new complete object and the member fight over the same bytes forever.

This is the deeper lesson: **field-granular borrowing is not expressible in the standard's
object algebra in either direction.** Any sequence of blessed lifetime operations either kills
something that must stay alive or names the wrong object. There is no incantation; the P0593
authors knew this, and every production allocator (glibc in C, tcmalloc and jemalloc in C++)
lives in the same spot. What actually carries correctness is the temporal-handoff discipline of
§2 and §6, guaranteed by implementations and matching C's effective-type model.

`start_lifetime_as` **is** the right tool where its reuse-everything semantics are harmless —
windows where nothing conflicting is alive:

- **Top-chunk formation** (AfMalloc.cpp:760–761): at that point the bytes past `af_arena.top_`
  hold nothing live (the user hasn't been handed their pointer yet), so
  `std::start_lifetime_as<Chunk>(af_arena.top_)` is safe and makes the top chunk's lifetime
  explicit instead of leaning on mmap's implementation-defined implicit creation.
- Fresh mmap'd heap setup, before anything is parceled out.

## 5. Is `std::launder` the answer, then?

**Yes — for the pointers; it neither can nor needs to fix the field.** The division of labor:

`std::launder(reinterpret_cast<Chunk*>(addr))` **recovers** a pointer to an object that is
*already alive* at `addr`. It creates nothing, ends nothing, writes nothing — zero blast
radius, safe next to live neighbors. Its precondition is that a `Chunk` is within its lifetime
at that address — which, under the narrow reading of §3, is exactly true everywhere this code
manufactures chunk pointers from `uintptr_t` arithmetic. What launder fixes is the
**provenance/legality of the pointer itself**: a pointer computed via integer round-trips is
not, in the abstract machine, automatically a pointer *to* the object living there; launder
makes it one.

Use it (e.g., via one helper) at every arithmetic-minted chunk pointer where a live `Chunk`
exists by construction:

```cpp
inline Chunk* chunkAt(void* addr) {
    return std::launder(reinterpret_cast<Chunk*>(addr));
}
```

- `free()`'s entry, AfMalloc.cpp:286 — the `Chunk` was placement-new'd in `malloc`, definitely
  alive;
- `moveToTheNextChunk` / `moveToThePreviousChunk` results used as live chunks
  (AfMalloc.cpp:23–37);
- `getHeapAddress` (AfMalloc.hpp:264–271) — the `AfHeap` was `construct_at`'d in
  `allocateNewHeap`; recover it with launder rather than restarting it with
  `start_lifetime_as`.

What launder does **not** do: revive `previous_size_`. It never creates objects, so it cannot
close the §3/§4 formal gap — but per §3 that gap needs no closing: the `Chunk` stays alive
(narrow reading), the store in `setPrevSize` re-establishes the field's value exactly as C's
effective-type rule describes, and no read ever precedes that store.

## 6. The invariants that actually carry the proof

The safety of the borrow rests on discipline, not incantations. These must hold (and do):

- **I1 — Borrow cap.** The overshoot into the next chunk is ≤ 8 bytes
  (`needed_size = align16(size + 8)` ⇒ `c+16+size ≤ next+8`), so `size_` and the `PREV_FREE`
  bit at `[next+8, next+16)` are never user-touched. This is what makes the flag readable at
  all times.
- **I2 — Write-before-read, flag-gated.** `previous_size_` is read (AfMalloc.cpp:310, 316) only
  under `isPrevFree()`, and the flag is raised only immediately after `setPrevSize` wrote the
  field (AfMalloc.cpp:359–363). Every read observes a value stored after the last handoff.
- **I3 — No metadata reads during user tenure.** `prev_`/`next_` of an *allocated* chunk are
  never accessed (those bytes belong to that chunk's user); only free-list operations on *free*
  chunks touch them.
- **I4 — Handoffs only at malloc/free boundaries.** Every ownership transition of a byte is a
  storage-reuse event at a well-defined point; no byte is ever interpreted under two types
  between two consecutive handoffs.

Suggested hardening (cheap, none change codegen):

1. `chunkAt()` helper with `std::launder` (§5) used at all arithmetic-minted chunk pointers.
2. `std::start_lifetime_as<Chunk>` at top-chunk formation (§4) — the one place explicit
   creation is both needed and harmless.
3. Keep `static_assert(std::is_implicit_lifetime_v<Chunk>)` (AfMalloc.hpp:176) and add the same
   for `AfHeap` — these are the preconditions of the whole model.
4. A comment on `getMallocNeededSize` naming I1 explicitly (the existing diagram shows the
   borrow; state the ≤ 8-byte cap and why `size_` is therefore safe).
5. Empirical checks: UBSan (`-fsanitize=undefined`) and TySan (`-fsanitize=type`) runs — TySan
   tracks per-byte effective types and was clean on this allocator, consistent with §3(e).

## 7. Verdict

- The borrow does **not** create two simultaneously live overlapping objects — [basic.life]/(1.5)
  makes that unrepresentable; every transition is a sequenced handoff.
- The residual formal question (partial reuse) resolves to **subobject-only death** by text,
  intent, C parity, and implementation reality; the containing `Chunk` stays alive and its
  member accesses remain valid.
- **Do not** `start_lifetime_as` an allocated neighbor — it formally kills live user data.
  **Do** `launder` arithmetic-minted pointers to live chunks, and reserve `start_lifetime_as`
  for windows where nothing is alive (top-chunk formation, fresh heaps).
- The design is the same one every production allocator uses; its correctness proof is the
  invariant set in §6, which the code already satisfies.
