## README IS OUT OF DATE!

### ABOUT
growt (GrowTable) is a header only library implementing a concurrent growing hash table.  There are different variants depending on the use case. See explanation below.

Using the supplied cmake build system you can compile an example file (example/example.cpp) and a multitude of different tests/benchmarks. These were also used to generate the plots displayed in our scientific publication => https://arxiv.org/abs/1601.04017 (slightly outdated).


### USAGE OF THIRD PARTY CODE
This package can be used all on its own (see example.cpp and ...test.cpp).  However third party codes are used for additional functionality/tests.

##### We use the following libraries:
###### for utility:
- TBB - to implement a fixed memory pool allocator
- xxHash - usable hash function

###### as third party hash tables (for benchmarks):
- TBB - ```tbb::concurrent_hash_map and tbb::concurrent_unordered_map```
- LibCuckoo - ```cuckoohash_map```
- Junction - ```junction::ConcurrentMap_Linear ..._Grampa ..._Leapfrog```
- Folly - ```folly::AtomicHashMap```


### BUILD NOTES]
Tested using current versions of g++.

##### Easy build without third party code

```bash
git clone https://github.com/TooBiased/growt.git
cd growt
mkdir build
cd build
cmake ..
# this will probably display some messages about not finding certain
# third party packages and therefore replacing some functionality
make
```


##### Building with third party libraries
Third party libraries are either installed using your package manager or downloaded into an `extern` folder next to the project folder. This folder can either be added to the `PATH` environment variable, or other appropriate environment variables have to be set in order to find the correct libraries.

```bash
git clone https://github.com/TooBiased/growt.git

# Create a folder for (third party code that is used by our library)
# No third party code is needed
# benefits: performance tests for other hash tables, better hash functions, better allocators
mkdir extern
cd extern

# Install TBB (third party hash table + necessary for our pool allocators)
sudo apt-get install libtbb-dev

# Download libcuckoo (third party hash table)
git clone https://github.com/efficient/libcuckoo.git
cd libcuckoo
autoreconf -fis
./configure
make
make install
cd ..

# Download junction/turf (third party hash table)
# they are built as special targets using our cmake build system
git clone https://github.com/preshing/junction.git
git clone https://github.com/preshing/turf.git
# set an environment variable such that the junction and turf libraries
# can be found by our cmake script
export JUNCTION_ROOT=$PWD

# Download folly (third party hash table)
git clone https://github.com/facebook/folly.git
## follow their instructions to install (read their README)
##  download gtest and unzip it into folly/test
##  install necessary libraries
cd folly
autoconf
./configure
make
make install
cd ..

# Download xxHash (hash function)
git clone https://github.com/Cyan4973/xxHash.git
# set an environment variable such that the xxHash code
# can be found by our cmake script
export XXHASH_ROOT=$PWD

# Download smhasher (hash function)
git clone https://github.com/aappleby/smhasher.git
# set an environment variable such that the xxHash code
# can be found by our cmake script
export SMHASHER_ROOT=$PWD


# Return to main project go to build folder and run cmake
cd ../growt
mkdir build
cd build
cmake ..
# alternatively use ccmake .. to change options
make
# or make <specific_test> (see below)

```

### Content
This package contains many different hash table variants. You can find some example instanciations in `data-structures/definitions.h` (alternatively look at `tests/selection.h`, which is used to select a hash table at compile time using compile time definitions).

All our data structures and connected classes are declared in the `growt` namespace.

##### Non-growing hash tables
- `folklore ` (or `Circular<SimpleElement, HASHFUNCTION, ALLOCATOR>`)
is a simple linear probing hash table using atomic operations, to change cell contents.

- `xfolklore` (or `TSXCircular<SimpleElement, HASHFUNCTION, ALLOCATOR>`)
similar to Folklore, but uses Intel TSX transactional memory extensions to ensure atomicity instead of atomics.
Your machine has to support Intel TSX transactions for this to work (check your cpu-flags and try compiling with `-mrtm`)

##### Growing hash tables
Our growing variants use the above non-growing tables. They grow by migrating the entire hash table once it gets too full for the current size. Migration is done in the background without the user knowing about it. During the migration hash table accesses may be delayed until the table is migrated (usually the waiting thread will help with the migration).

Threads can only access our growing hash tables by creating a thread specific handle. These handles cannot be shared between threads.

- `uaGrow   ` (or `GrowTable<Circular<MarkableElement, HASHFUNCTION,ALLOCATOR>, WStratUser, EStratAsync>`),
is a growing table, where threads that access the table are responsible for eventual migrations. These will be performed automatically and asynchronously. Migrated cells are marked to ensure atomicity (this reduces the available key space by one bit. Keys >=2^63 cannot be inserted).

- `usGrow   ` (or `GrowTable<Circular<SimpleElement, HASHFUNCTION,ALLOCATOR>, WStratUser, EStratSync>`),
similar to `uaGrow` but growing steps are somewhat synchronized (ensures automatically that no updates run during growing phases) eliminating the need for marking.

- `paGrow   ` (or `GrowTable<Circular<MarkableElement, HASHFUNCTION,ALLOCATOR>, WStratPool, EStratAsync>`),
where growing is done by a dedicated pool of growing threads. Similar to `uaGrow` marking is used to ensure atomicity of the hash table migration.

- `psGrow   ` (or `GrowTable<Circular<SimpleElement, HASHFUNCTION,ALLOCATOR>, WStratPool, EStratSync>`),
combining the thread pool of `paGrow` with the synchronized growing approach of `usGrow`.

All growing variants can also be built using the `TSXCircular` table instead of `Circular`.
For a more in-depth description of our growing variants, check out our paper (https://arxiv.org/abs/1601.04017).

##### Our tests and Benchmarks
All generated tests (`make` recipes) have the same name structure.

`<test_abbrv>_<hash_table_name>` => e.g. `in_uaGrow`

###### test_abbrv
- `in ` - insertion and find test (seperate)
- `mix` - mixed inserts and finds
- `agg` - aggregation using insertOrUpdate on a skewed key sequence
- `co ` - updates and finds on a skewed key sequence
- `del` - alternating inserts and deletions (approx. constant table size)

###### full list of hash tables
Some of the following tables have to be activated through cmake options.
- `sequential` - our sequential table (use only one thread!)
- `folklore` - our non growing tables
- `uaGrow, usGrow, paGrow, psGrow` - our main growing tables
- `usnGrow, psnGrow` - two alternate growing variants should behave similar to `usGrow` and `psGrow`
- `xfolklore, uaxGrow, usxGrow, paxGrow, psxGrow, usnxGrow, psnxGrow` - tsx variants of previous tables
- `junction_linear, junction_grampa, junction_leapfrog, folly, cuckoo, tbb_hm, tbb_um` - third party tables

Note: in the paper we have some additional third party hash tables. These depend on some additional wrappers and are not reproduced here. They might be added in the future.

### Usage in your own projects

##### Including our project
To make it easy, you can include the header `data-structures/definitions.h`, which includes all necessary files and offers the nice type definitions used above. Of course, this increases the size of your compilation unit significantly -- eventually slowing down compile times. If this is a concern you can include files directly and define the specific hash table. See `tests/selection.h` for examples of this technique. This header is used to choose a specific hash table and the corresponding includes depending on a compile time constant (e.g. `-D UAGROW`).

Example:
```cpp
#include "data-structures/markableelement.h"
#include "data-structures/circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/growtable.h"
using uaGrow = GrowTable<Circular<MarkableElement, HASHFCT, ALLOCATOR<MarkableElement> >,
                         WStratUser, EStratAsync>
```

##### Hash table interface
All our hash tables support the following functionality:
*called on global object*
- `HashTable(size_t n)` - constructor allocating a table large enough to hold `n` elements.
- `HashTable::Handle getHandle()` - create handles that can be used to access the table (alternatively `HashTable::Handle(HashTable global_table_obj)`)


*called through handles*
- `ReturnCode insert(uint64_t k, uint64_t d)` - inserts data element `d` at key `k`. Does nothing if that key is already present (see ReturnCode).
- `ReturnCode update(uint64_t k, uint64_t d, UpdateFunction f)` - looks for an element with key `k`. If present, the stored data is changed atomically using the supplied update function (instruction below).
- `ReturnCode insertOrUpdate(uint64_t k, uint64_t d, UpdateFunction f)` - combines the two functions above. If no data was present with key `k`, then `d` is inserted, otherwise the stored data is updated. The result specifies which has happend.
- `ReturnCode remove(uint64_t k)` - removes the stored key value pair (memory is reclaimed once the table is migrated).
- `ReturnElement find(uint64_t k)` - finds the data element stored at key `k` (`bool(ReturnElement e) = find successful` `ReturnElement.first = key or 0` `ReturnElement.second = data or undefined`) can be untied using `std::tie` (but only if types match exactly).

Using handles is not necessary for our non-growing tables.

##### About UpdateFunctions
Our update interface uses a user implemented update function. Any object given as an update function should have an `operator()(uint64_t& cur_data, uint64_t key, uint64_t new_data)`. This function will usually be called by our update operations to change a stored elements data field. This operations does not have to be atomic: it is made atomic using CAS operations. When an atomic version of the method exists, then it can be implemented as an additional function called `.atomic(uint64_t& cur_data, uint64_t key, uint64_t new_data)`, which is only called iff atomics can be called safely (mostly in synchronized growing variants `usGrow` and `psGrow`). Presensce of the atomic variant is detected automatically. An example for a fully implemented `UpdateFunction` might look like this:

```cpp
struct Increment
{
    void operator()(uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        lhs += rhs;
    }

    // an atomic implementation can improve the performance of updates in .sGrow
    // this will be detected automatically
    void atomic    (uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        __sync_fetch_and_add(&lhs, rhs);
    }

};
```

##### About our utility functions
`utils/alignedallocator.h` - a very simple allocator returning only aligned data elements.

`utils/poolallocator.h` - in many of our growing tests, mapping virtual to physical memory has been a bottleneck. Therefore, we use this allocator it starts by allocating a big amount of memory and uses it as a memory pool for future allocations. Memory mapping is forced in the beginning by writing into the buffer. Different variants of this allocator are available using different malloc variants to allocate the buffer (malloc, libnuma interleaved allocation, huge TLB page allocator).

`utils/hashfct.h` - some different hash functions the correct implementation is chosen at compile time according to a compile time constant.

`utils/counting_wait.h`, `utils/test_coordination.h`, `utils/thread_basics.h` - all implement some threading capabilities mostly used to simplify writing tests/benchmarks. But also necessary for our thread pool growing variants.

`utils/keygen.h` - create numbers with a zipf distribution (used to create contentious test instances)

`utils/commandline.h` - simple command line parser, to read user inputs (used for our tests).
