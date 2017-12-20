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

#include "clok.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#define NUM_SLOTS             (UCHAR_MAX)
#define RAND_COUNT            (128)
#define CYCLE_DETECT_INTERVAL (4)

// random number array used for quick randomization
static const unsigned RAND_NUMS[RAND_COUNT] =
{
    0x7e176142, 0xc640db5e, 0x281de131, 0x22ac35a8,
    0xf04a5af9, 0x91fbac16, 0xd056cd18, 0x85b2f13d,
    0x3ee31dfb, 0x4d572678, 0xe96817f2, 0xb85ebfe4,
    0xacf89c40, 0x7c4d37dd, 0x9bdea20f, 0x0cba2c81,
    0xe9c9db60, 0x1c87c8ba, 0x6121348a, 0xaa424ca4,
    0x345c31ef, 0xf4e8fda7, 0x80d7b8c3, 0x7da72b7f,
    0x8ecd460d, 0x1d1b8af2, 0x3bf0c0ac, 0x89cd1621,
    0x017e9563, 0xc2e4c531, 0x4510975e, 0x4c69166a,
    0x6d834b93, 0xd9420fc2, 0x04c14a25, 0x6c2cf2c3,
    0x74013283, 0x4a72ebd5, 0x0ad805e8, 0x8df9b7cf,
    0xda7203c8, 0xdfd07140, 0x274f88e7, 0x79187ac6,
    0xd95f9e20, 0xbcfd2b4f, 0xc9994696, 0x7d573e85,
    0x55d8c6a7, 0x05a1c3d5, 0x74c57c8c, 0xb01565e6,
    0xf48ebf0f, 0x0b858386, 0x7a3cfdf8, 0xfaf3ea70,
    0x93797505, 0xcfd40777, 0x47529b83, 0xf2e2b32a,
    0xd7246c72, 0x1e5baf35, 0x3c1c0e7f, 0x51175781,
    0xc1568e0f, 0x182420e8, 0x907719bd, 0x6a54d158,
    0xd68bd46f, 0x8d1c9c64, 0x3466778c, 0xe8e0f070,
    0x5f2a76f8, 0xc50b4a81, 0x75cfd99e, 0x27ede0f2,
    0x28ca3c34, 0x387ee0d6, 0x66d54f49, 0x07a111b4,
    0x4f02dc16, 0x91f8cbd9, 0x55b94442, 0xe1a3330c,
    0xb6c91707, 0xf3572e66, 0x1e621125, 0xf900875c,
    0x809b6129, 0xa0d856d4, 0x147466a0, 0x65d83705,
    0x23d258a0, 0x7df2f7ef, 0xcd278ef5, 0x8ddb32fe,
    0xcd0a49cd, 0xcd8c843a, 0xd36b7575, 0x73730f7f,
    0x21489b9e, 0xf194fdf7, 0x03a829db, 0x013f87b7,
    0xd4a95060, 0x25969f0b, 0xbb2d2f51, 0x94c74824,
    0xe76d472e, 0x13dbe13e, 0x81c3edd7, 0x3318bcda,
    0x1bf54a49, 0x059e6008, 0x4a39f9c8, 0x04115690,
    0x659985ab, 0x9d1aec91, 0x669cc20f, 0xeabd4f26,
    0xb8e31df4, 0x6edccd51, 0x13353882, 0x1bbd78e8,
    0x7eee6944, 0xc2e93bc5, 0xa55f4d3a, 0xf4235b90
};


typedef struct BlockSlim   BlockSlim;
typedef struct BlockFat    BlockFat;
typedef struct Schedule    Schedule;

typedef unsigned char  uchar;
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef uchar          Slot;
typedef ushort         Desc;
typedef ushort         Size;

struct ck_Pool
{
    // user config
    ck_Config config;
    
    // schedules
    BlockSlim* eSchedule[NUM_SLOTS]; // expire
    BlockFat*  pSchedule[NUM_SLOTS]; // preserve
    
    // roots and orphan lists
    BlockSlim*   roots;
    BlockFat*    orphans;
    
    // other
    uint   clock;
    uint   rand;
    size_t used;
};


#if CK_SHOULD_PACK
#  pragma pack( push, 1 )
#endif

struct BlockSlim
{
    BlockSlim*    eNext;
    BlockSlim**   eRef;
    Slot          eSlot;
    Desc          desc;
    
    // data follows
};

struct BlockFat
{    
    // preservation scheduling
    BlockFat*   pNext;
    BlockFat**  pRef;
    Slot        pSlot;
    
    // cycle detection
    uchar       cycleCD;
    BlockFat*   owner;
    
    BlockSlim   slim;
    
    // data follows
};

#if CK_SHOULD_PACK
#  pragma pack( pop )
#endif


// helper prototypes
static inline void*
allocRaw( ck_Pool* pool, size_t size );

static inline BlockSlim*
ptrToSlim( void* ptr );

static inline BlockFat*
ptrToFat( void* ptr );

static inline BlockFat*
slimToFat( BlockSlim* slim );

static inline void*
fatToPtr( BlockFat* block );

static inline void*
slimToPtr( BlockSlim* block );

static inline BlockSlim*
fatToSlim( BlockFat* fat );

static inline Size
compressSize( size_t size );

static inline size_t
decompressSize( Size size );

static inline void
setSize( BlockSlim* block, Size size );

static inline void
setRoot( BlockSlim* block, bool isRoot );

static inline void
setFat( BlockSlim* block, bool isFat );

static inline void
setOrphan( BlockSlim* block, bool isOrphan );

static inline void
setDesc( BlockSlim* block, Size size, bool isFat, bool isRoot );

static inline void
setOwner( BlockSlim* block, BlockFat* owner );

static inline Size
getSize( BlockSlim* block );

static inline bool
isRoot( BlockSlim* block );

static inline bool
isFat( BlockSlim* block );

static inline bool
isOrphan( BlockSlim* block );

static inline void
eInsert( ck_Pool* pool, BlockSlim* block );

static inline void
eExtract( BlockSlim* block );

static inline void
pInsert( ck_Pool* pool, BlockFat* block );

static inline void
pExtract( BlockFat* block );

static inline void
doExpire( ck_Pool* pool, BlockSlim* block );

static inline void
doPreserve( ck_Pool* pool, BlockFat* block );

static inline void
toOrphan( ck_Pool* pool, BlockFat* block );

static inline void
toRoot( ck_Pool* pool, BlockSlim* block );

static inline bool
shouldRef( ck_Pool* pool, BlockFat* owner, BlockSlim* block );

static inline bool
isBefore( ck_Pool* pool, Slot before, Slot after );

static inline void
setPreserveSlot( ck_Pool* pool, BlockFat* block );


// api implementation
ck_Pool*
ck_makePool( ck_Config const* config )
{
    ck_Pool* pool = config->alloc( config->context, sizeof(*pool) );
    pool->config  = *config;
    pool->roots   = NULL;
    pool->orphans = NULL;
    pool->clock   = 0;
    pool->rand    = 0;
    pool->used    = 0;
    
    for( uint i = 0 ; i < NUM_SLOTS ; i++ )
    {
        pool->eSchedule[i] = NULL;
        pool->pSchedule[i] = NULL;
    }
    
    return pool;
}

void
ck_freePool( ck_Pool* pool )
{
    for( uint i = 0 ; i < NUM_SLOTS ; i++ )
    {
        while( pool->eSchedule[i] )
            doExpire( pool, pool->eSchedule[i] );
    }
    
    while( pool->roots )
        doExpire( pool, pool->roots );
    
    pool->config.free( pool->config.context, pool );
}

void*
ck_allocSlim( ck_Pool* pool, size_t size, void* owner )
{
    Size cSize = compressSize( size );
    if( size > decompressSize( cSize ) )
        // allocation too big
        return NULL;
    
    size_t     tSize = size + sizeof(BlockSlim);
    BlockSlim* block = allocRaw( pool, tSize );
    if( block == NULL )
        return NULL;

    setDesc( block,
             cSize,
             false,
             (owner == NULL ) );
    
    if( block == NULL )
        return NULL;
    
    if( owner )
    {
        setOwner( block, ptrToFat( owner ) );
        eInsert( pool, block );
    }
    else
    {
        toRoot( pool, block );
    }

    pool->used += tSize;
    return slimToPtr( block );
}

void* 
ck_allocFat( ck_Pool* pool, size_t size, void* owner )
{
    Size cSize = compressSize( size );
    if( size > decompressSize( cSize ) )
        // allocation too big
        return NULL;
    
    size_t     tSize = size + sizeof(BlockFat);
    BlockFat* block = allocRaw( pool, tSize );
    if( block == NULL )
        return NULL;

    setDesc( fatToSlim(block),
             cSize,
             true,
             (owner == NULL ) );
    
    // if owner non-NULL then block is not root and
    // needs it's expiration slot set
    if( owner )
    {
        setOwner( fatToSlim(block), ptrToFat( owner ) );
        eInsert( pool, fatToSlim(block) );
    }
    else
    {
        block->slim.eSlot = 0;
        toRoot( pool, fatToSlim(block) );
    }

    setPreserveSlot( pool, block );
    pInsert( pool, block );
    pool->used += tSize;
    return fatToPtr( block );
}

void
ck_ref( ck_Pool* pool, void* alloc, void* owner )
{
    if( alloc == NULL || alloc == owner )
        return;
    
    BlockSlim* block = ptrToSlim( alloc );
    if( isRoot( block ) )
        return;
    
    eExtract( block );
    
    if( !owner )
    {
        toRoot( pool, block );
        return;
    }
    
    BlockFat* oBlock = ptrToFat( owner );
    if( isFat( block ) )
    {
        // if referenced by any non-owner block
        // then we know it isn't in an orphan cycle
        BlockFat* fat = slimToFat(block);
        if( fat->owner != oBlock )
            fat->cycleCD = CYCLE_DETECT_INTERVAL;
    }
        
    if( shouldRef( pool, oBlock, block ) )
    {
        setOwner( block, oBlock );
        setOrphan( block, false );
        if( isOrphan( block ) )
            doPreserve( pool, slimToFat(block) );
    }
    
    eInsert( pool, block );
}

void
ck_unroot( ck_Pool* pool, void* alloc, void* owner )
{
    if( alloc == NULL || alloc == owner || owner == NULL )
        return;
    
    BlockSlim* block = ptrToSlim( alloc );
    if( !isRoot( block ) )
        return;
    
    eExtract( block );
    setOwner( block, ptrToFat( owner ) );
    eInsert( pool, block );
}

void
ck_cycle( ck_Pool* pool )
{
    uint ticks = NUM_SLOTS;
    while( ticks-- )
        ck_tick( pool );
}

void
ck_tick( ck_Pool* pool )
{
    Slot slot = pool->clock % NUM_SLOTS;
    
    BlockFat** pSlot = &pool->pSchedule[slot];
    while( *pSlot )
        doPreserve( pool, *pSlot );
    
    BlockSlim** eSlot = &pool->eSchedule[slot];
    while( *eSlot )
        doExpire( pool, *eSlot );
    
    pool->clock++;
}

void
ck_step( ck_Pool* pool )
{
    Slot slot = pool->clock % NUM_SLOTS;
    BlockFat** pSlot = &pool->pSchedule[slot];
    BlockSlim** eSlot = &pool->eSchedule[slot];
    
    if( *pSlot )
        doPreserve( pool, *pSlot );
    else
    if( *eSlot )
        doExpire( pool, *eSlot );
    else
        pool->clock++;
}

void
ck_reserve( ck_Pool* pool, size_t amount )
{
    uint ticks = NUM_SLOTS;
    while( pool->used + amount > pool->config.quota && ticks-- )
        ck_tick( pool );
}

size_t
ck_avail( ck_Pool* pool )
{
    return pool->config.quota - pool->used;
}

size_t
ck_used( ck_Pool* pool )
{
    return pool->used;
}


static inline void*
allocRaw( ck_Pool* pool, size_t size )
{
    if( size > pool->config.quota )
        return NULL;
    
    uint ticks = NUM_SLOTS;
    while( pool->used + size > pool->config.quota && ticks-- )
        ck_tick( pool );
    
    if( pool->used + size > pool->config.quota )
        return NULL;
    
    return pool->config.alloc( pool->config.context, size );
}

static inline BlockSlim*
ptrToSlim( void* ptr )
{
    return ptr - sizeof(BlockSlim);
}

static inline BlockFat*
ptrToFat( void* ptr )
{
    return ptr - sizeof(BlockFat);
}

static inline BlockFat*
slimToFat( BlockSlim* slim )
{
    return (BlockFat*)((void*)slim - (intptr_t)&((BlockFat*)0)->slim);
}

static inline BlockSlim*
fatToSlim( BlockFat* fat )
{
    return &fat->slim;
}

static inline void*
fatToPtr( BlockFat* block )
{
    return (void*)block + sizeof(BlockFat);
}

static inline void*
slimToPtr( BlockSlim* block )
{
    return (void*)block + sizeof(BlockSlim);
}



static inline Size
compressSize( size_t size )
{
    // compressed size fits into 13 bytes,
    // the highest 5 bits represent a base
    // 2 exponent and the lower 8 represent
    // an byte offset; so the actual size
    // represented is (1 << high5 + low8)
    uint low  = size & 0xFF;
    uint high = 0;
    
    size >>= 8;
    while( size )
    {
        high++;
        high >>= 1;
    }
    
    // if (high == 0) then we can take one byte
    // away from (low) since we know that one
    // byte is implied
    if( high == 0 && low != 0 )
        low--;
    
    return high << 8 | low;
}

static inline size_t
decompressSize( Size size )
{
    return (1 << (size & 0x1FF << 8)) + (size & 0xFF);
}

static inline void
setSize( BlockSlim* block, Size size )
{
    block->desc &= 0x7;
    block->desc |= size << 3;
}

static inline void
setRoot( BlockSlim* block, bool isRoot )
{
    if( isRoot )
        block->desc |= 2;
    else
        block->desc &= ~2;
}

static inline void
setFat( BlockSlim* block, bool isFat )
{
    if( isFat )
        block->desc |= 1;
    else
        block->desc &= ~1;
}

static inline void
setOrphan( BlockSlim* block, bool isOrphan )
{
    if( isOrphan )
        block->desc |= 4;
    else
        block->desc &= ~4;
}

static inline void
setDesc( BlockSlim* block, Size size, bool isFat, bool isRoot )
{
    block->desc = 0;
    setSize( block, size );
    setFat( block, isFat );
    setRoot( block, isRoot );
}

static inline void
setOwner( BlockSlim* block, BlockFat* owner )
{
    block->eSlot = owner->pSlot;
    if( isFat( block ) )
    {
        BlockFat* fat = slimToFat( block );
        fat->owner    = owner;
    }
}

static inline Size
getSize( BlockSlim* block )
{
    return block->desc >> 3;
}

static inline bool
isRoot( BlockSlim* block )
{
    return block->desc & 2;
}

static inline bool
isFat( BlockSlim* block )
{
    return block->desc & 1;
}

static inline bool
isOrphan( BlockSlim* block )
{
    return block->desc & 4;
}

static inline void
eInsert( ck_Pool* pool, BlockSlim* block )
{
    BlockSlim** ePtr = &pool->eSchedule[block->eSlot];
    block->eNext = *ePtr;
    block->eRef  = ePtr;
    if( block->eNext != NULL )
        block->eNext->eRef = &block->eNext;
    *ePtr = block;
}

static inline void
eExtract( BlockSlim* block )
{
    *block->eRef = block->eNext;
    if( block->eNext != NULL )
        block->eNext->eRef = block->eRef;
    block->eRef  = NULL;
    block->eNext = NULL;
}

static inline void
pInsert( ck_Pool* pool, BlockFat* block )
{
    BlockFat** pPtr = &pool->pSchedule[block->pSlot];;
    block->pNext = *pPtr;
    block->pRef  = pPtr;
    if( block->pNext != NULL )
        block->pNext->pRef = &block->pNext;
    *pPtr = block;
}

static inline void
pExtract( BlockFat* block )
{
    *block->pRef = block->pNext;
    if( block->pNext != NULL )
        block->pNext->pRef = block->pRef;
    block->pRef  = NULL;
    block->pNext = NULL;
}

static inline void
doExpire( ck_Pool* pool, BlockSlim* block )
{
    eExtract( block );
    if( isFat( block ) )
    {
        BlockFat* fat = slimToFat( block );
        pExtract( fat );
    }
    
    
    if( pool->config.expire )
        pool->config.expire( pool->config.context, slimToPtr( block ) );
    
    pool->used -= decompressSize( getSize( block ) );
    if( isFat( block ) )
    {
        pool->used -= sizeof(BlockFat);
        pool->config.free( pool->config.context, slimToFat(block) );
    }
    else
    {
        pool->used -= sizeof(BlockSlim);
        pool->config.free( pool->config.context, block );
    }
}

static inline void
doPreserve( ck_Pool* pool, BlockFat* block )
{
    pExtract( block );
    
    // move the block's preservation slot to the next cycle
    setPreserveSlot( pool, block );
    
    pInsert( pool, block );
    
    // cycle detection, if we find a cycle then
    // we orphanize all the blocks involved; if
    // one of them receives a ref from outside
    // the cycle then all will be unorphanized
    // by an immediate preservation
    block->cycleCD--;
    if( block->owner != NULL && block->cycleCD == 0 )
    {
        BlockFat* oIter = block->owner;
        while( oIter != NULL && oIter != block )
            oIter = oIter->owner;
        if( oIter == block )
        {
            do
            {
                BlockFat* orphan = oIter;
                oIter = oIter->owner;
                toOrphan( pool, orphan );
            } while( oIter != block );
        }
        
        block->cycleCD = CYCLE_DETECT_INTERVAL;
    }
    
    
    if( pool->config.preserve )
        pool->config.preserve( pool->config.context, fatToPtr(block), pool );
}

static inline void
toOrphan( ck_Pool* pool, BlockFat* block )
{
    pExtract( block );
    setOrphan( fatToSlim(block), true );
    block->pNext = pool->orphans;
    block->pRef  = &pool->orphans;
    if( block->pNext != NULL )
        block->pNext->pRef = &block->pNext;
    pool->orphans = block;
    block->owner  = NULL;
}

static inline void
toRoot( ck_Pool* pool, BlockSlim* block )
{
    setRoot( block, true );
    block->eNext = pool->roots;
    block->eRef  = &pool->roots;
    if( block->eNext != NULL )
        block->eNext->eRef = &block->eNext;
    pool->roots = block;
    
    if( isFat( block ) )
    {
        BlockFat* fat = slimToFat(block);
        fat->owner  = NULL;
    }
}

static inline bool
shouldRef( ck_Pool* pool, BlockFat* owner, BlockSlim* block )
{
    if( isOrphan( block ) )
        return true;
    if( isBefore( pool, block->eSlot, owner->pSlot ) )
        return true;
    return false;
}

static inline void
setPreserveSlot( ck_Pool* pool, BlockFat* block )
{
    // preserve slot is set to a random slot after
    // the expiration slot; since we have a limited
    // number of slots, the 'after' means that the
    // slot is in the range between the block's
    // expiration time and the current clock, starting
    // at the expiration and overflowing at the max.
    // This actually results in some clustering of expiration
    // events as the distance between the current clock
    // and expiration increase, but the alternative is
    // complete randomization which results in objects
    // living for much longer than necessary sice the propegation
    // of a dropped reference could take up to NUM_SLOTS^CHAIN_LEN
    // ticks; where CHAIN_LEN is the number of references from
    // the initial drop to the end of the chain
    
    Slot now = pool->clock % NUM_SLOTS;
    Slot rnd = RAND_NUMS[pool->rand++ % RAND_COUNT] % NUM_SLOTS;
    Slot max = now - 2;
    Slot min;
    if( isRoot(fatToSlim(block)) )
        min = now + 1;
    else
        min = block->slim.eSlot;
    
    if( min == max )
        block->pSlot = min;
    else
    if( max > min )
        block->pSlot = (min + rnd % (max-min)) % NUM_SLOTS;
    else
        block->pSlot = (min + rnd % (NUM_SLOTS-min+max)) % NUM_SLOTS;
}

static inline bool
isBefore( ck_Pool* pool, Slot before, Slot after )
{
    Slot slot = pool->clock % NUM_SLOTS;
    if( slot < after && before >= slot && before < after )
        return true;
    if( slot > after && before < after )
        return true;
    if( slot > after && before >= slot )
        return true;
    
    return false;
}

