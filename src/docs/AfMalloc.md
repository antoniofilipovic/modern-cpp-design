## AfMalloc how does it work



## How to deal when allocation comes and we have no space in the top chunk which is current one
Probably we should allocate new heap, and deal with allocation from that heap.
That leaves that old top chunk in position which we need to deal with. We would need to put our top chunk
back to the free chunk list. 
When slicing top chunk we need to know how much we can slice, there is one possible option here.

We might slice it even up before and put on the small chunk the flag for border chunk. In that case we would actually
put in the free chunk list the big and small chunk, rest and border. In the case rest gets chopped up later the only 
interaction it would have with the border chunk is to check if that chunk is coalescable. That chunk would not be 
coalescable since it is border chunk.
The second case for chunk not to be colescable is if it is topChunk and third one is if it is fast chunk.

In that case a lot of things I have written below die.

## Anatomy of the chunk
Each chunk consists of following fields, prev_size, size, ptr_next, ptr_prev. The total size of the chunk is at minimum
32 bytes. Chunk can be either free or at use. If the chunk is at use, ptr_next and ptr_prev are used, together with the
next chunk's prev_size field. This means chunk is expanding beyond its own space and can count on 24 bytes at least to 
be used. If the chunk is free size is used, and ptr_next and ptr_prev are also used because chunk is most likely linked 
in some of the data structures which contain free lists of chunks for reusing of those freed chunks. When chunk is 
free the next chunk will use prev_size to mark what is the size of previous chunk which is free. However, chunk
being free does not mean it can be reused for later, and does not mean it is coalescable. 

There are different kind of chunks we have depending on their size, and on the general usage patterns.

### Border chunk
When there is no space to allocate new chunk from the current top chunk, it usually means we need to go for 
new heap. That would create a new top chunk and a mode of two top chunks. To avoid such position we need to use only 
one such chunk as a top chunk and park the other chunk into the free list of the chunks. 




### Top chunk


### Fast chunk

### Small chunk


### Large chunk



## Free chunk lists


### I think this can be skipped
For allocated chunk we set the size and for the free chunk we put size of that chunk. 
From that alone we have size. 

If the chunk bordering edge is freed, we can put prev_size but will need to put at one point that size of that chunk is 0
so it does not try to move further away.

so we can have border chunk with size 0 and prev_size can be set there
                                                                      border_chunk  
[current_alloc           ][top chunk which is new free large chunk][  size ]


                                                                      border_chunk  
[current_alloc           ][alloc][free ][alloc][free chunk           ][  size ]


                                                                      border_chunk  
[current_alloc           ][alloc][free ][alloc][alloc                ][  size ]
alloc when freed you ask is next free by moving to next chunk
next chunk is border chunk with size 0
so you ask move to the next chunk. since you cannot move there you stay at the same chunk
isPrevFree can't be asked then


we have this problem only
src/allocators/AfMalloc.cpp:395

maybe we can put another flag?

The flag we could put is border_flag. Since size of std::size_t is 8bytes, or 64 bits, we can put the second to last
bit to 1.
With that configuration 2^62 is the value of that number. 2^10 is 1kB, 2^20 is 1MB, then 2^30 is 1GB, then 2^40 is 1TB.
Hence we are safe for now with prices of the RAM.

