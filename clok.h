/*
 * MIT License
 * 
 * Copyright (c) 2017 Ray Stubbs
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef clok_h
#define clok_h
#include <stddef.h>

/* set to 1 to enable packing of block headers,
 * this can significantly reduce overhead but will
 * also slow down the collection a bit, and is
 * possibly unsafe on some platforms... though should
 * be fine for the common ones.  set to 0 to disable.
 */
#ifndef CK_SHOULD_PACK
#  define CK_SHOULD_PACK (0)
#endif

typedef struct ck_Pool  ck_Pool;
typedef struct ck_CbSet ck_CbSet;
typedef struct ck_Config ck_Config;

struct ck_Config
{
    /* amount of memory the pool is
     * allowed to have allocated at
     * once
     */
    size_t quota;
    
    /* the allocator to use for pool allocations,
     * Clok is purely a garbage collector, it doesn't
     * do other memory management tasks (like defrag)
     * so the provided allocator is expected to do all
     * the low level stuff; a wrapper around malloc is
     * generally sufficient.  The 'context' parameter
     * just passes the config's 'context' field, it's
     * a means of supplying user data to the callbacks
     */
    void* (*alloc)( void* context, size_t size );
    
    /* memory free callback, the counterpart to 'alloc' */
    void (*free)( void* context, void* alloc );
    
    /* expiration callback, this is called whenever an
     * allocation is about to be freed by the GC; it allows
     * the user to do some cleanup on the object, but leaves
     * it too the user to determin which type of object 'alloc'
     * is.  For a scripting language this would generally be
     * stored in some kind of generic type field
     */
    void (*expire)( void* context, void* alloc );
    
    /* preservation callback, this is called whenever an
     * allocation is due to refresh (re-ref) it's referenced;
     * the callback will be called perdiodically for each
     * 'fat' allocation and is expected to call the ck_ref
     * API function on each of the referenced maintained by
     * the allocation.  If the referenced allocations aren't
     * re-referenced at every 'preserve' period then they may
     * die while still in use.  So a proper preservation callback
     * is imperative for proper Clok operation.
     */
    void (*preserve)( void* context, void* alloc, ck_Pool* pool );
    
    /* the 'context' is just userdata passed to each of the
     * Clok callbacks; it allows users to keep track of some
     * non-global state from within the callbacks; if using
     * malloc/free wrappers for the memory callbacks then
     * NULL is generally sufficient.
     */
    void* context;
};

/* allocates a new memory pool given the
 * specified config struct; the size of
 * the pool will not be counted toward
 * the allocation quota.
 */
ck_Pool*
ck_makePool( ck_Config const* config );

/* deallocates the memory pool; note that
 * this'll free all of the pool's allocations,
 * so any externally held referenced should be
 * discarded after freeing
 */
void
ck_freePool( ck_Pool* pool );


/* allocates a 'slim' allocation; this is an atomic block
 * without any references, so it doesn't get a 'preserve'
 * period.  there's no need to maintain 'preserve' state
 * for these so the header overhead is smaller than for
 * 'fat' blocks; but still quite large, generally between
 * 12 and 24 bytes. the 'owner' is another of the pool's
 * blocks which is expected to be the one requesting the
 * allocation, and thus the initial reference holder of
 * the allocations.  if the owner is NULL then the block
 * is allocated as a 'root' node, and thus will never
 * expire unless unrooted.
 */
void*
ck_allocSlim( ck_Pool* pool, size_t size, void* owner );

/* allocates a 'fat' allocations; this is a non-atomic block,
 * meaning it maintains some references to other blocks in the
 * pool.  we need to maintain some additional state for these
 * blocks, so the memory overhead is a bit higher than for 'slim'
 * blocks; generally between 28 and 64 bytes.  See 'ck_allocSlim'
 * for description of 'owner'.
 */
void* 
ck_allocFat( ck_Pool* pool, size_t size, void* owner );


/* references an object, expanding its expiration time
 * by the owner's (referencing object's) presevation
 * time.  this should be called any time on object
 * 'owner' obtains a reference to another 'alloc'; and
 * should also be called from within the 'preserve' callback
 * to extend the expiration of the reference.  if not called
 * at appropriate times then the allocation may be freed
 * while still referenced.  an 'owner' of NULL promotes an
 * alloction to 'root' status, which prevent it from ever
 * expiring unless 'unrooted'.
 */
void
ck_ref( ck_Pool* pool, void* alloc, void* owner );

/* unroots an allocation, the 'owner' field should
 * be the only referencing object at the time of
 * demotion.  the 'ck_ref' function should be called
 * after the 'ck_unroot' function and before any of
 * the collection or allocation functions on each
 * additional reference in existance upon demotion.
 * if the 'owner' argument is the same as 'alloc'
 * the function will do nothing; so the owner should
 * not be the allocation itself.
 */
void
ck_unroot( ck_Pool* pool, void* alloc, void* owner );

/* invokes a full garbage collection cycle, in most
 * cases this will collect all garbage in the pool,
 * unless additional allocations are made in 'expire'
 * or 'preserve' callbacks.  there are some exceptions
 * where a block may escape collection in a cycle, but
 * these cases are rare and can generally be ignored
 */
void
ck_cycle( ck_Pool* pool );


/* invokes a single tick of collection, this is generally
 * (but not strictly) about 1/255th of all garbage in the
 * pool, collection time is generally much less than a cycle
 * and more than a step.
 */
void
ck_tick( ck_Pool* pool );

/* invokes a single step of collection, this is a single
 * garbage collection action, either invoking the 'preserve'
 * callback, invoking the 'expire' callback, or incrementing
 * the tick counter.  invokation of this function should be
 * virtually instantaneous, though the compute time of the
 * callbacks will play a role.
 */
void
ck_step( ck_Pool* pool );

/* invokes the ck_tick() function until the specified amount
 * of room is available in the quota.  will performa at most
 * one cycle of collection, if the target amount hasn't been
 * reached by then then will return anyway; so the availability
 * should be comfirmed with ck_avail().
 */
void
ck_reserve( ck_Pool* pool, size_t amount );

/* return the number of bytes left in the quota */
size_t
ck_avail( ck_Pool* pool );

/* return the number of bytes currently allocated */
size_t
ck_used( ck_Pool* pool );

#endif
