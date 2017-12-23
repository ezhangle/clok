# Clok: An Unconventional Garbage Collector
An unconventional garbage collector based on the idea of expiring/timed
references. The basic idea is that an object reference lasts only a
certain period of 'time' (gc ticks) after which the object will expire
if the reference isn't refreshed.  The idea that a user need
contstantly refresh references seems kinda' obsurd at first, but if the
task is designated to a callback which is called by the collector
itself; this is more reasonable, the callback is really just a generic
representation for a reference scanner.

## Usage
The Clok API is pretty minimal.  The only header file needed is
'clok.h' and the implementation is all in 'clok.c'; the 'main.c'
file is basically just a playground I used for basic testing and
isn't really a part of Clok.

To get started you'll need to create a memory
pool with the 'ck_makePool' function, passing it a config struct
to specify specific parameters:

    ck_Pool* ck_makePool( ck_Config const* config );

The config struct looks like this:

    struct ck_Config
    {
        size_t quota;
        void* (*alloc)( void* context, void* old, size_t size );
        void (*expire)( void* context, void* alloc );
        void (*preserve)( void* context, void* alloc, ck_Pool* pool );
        void* context;
    };

It mostly consists of callbacks for helping Clok do things that
it otherwise doesn't know how to.

The 'quota' field indicates the maximum amount of memory (in bytes)
that the pool is allowed to have allocated at once; if the next
allocation would brind the pool above this max then the pool is
forced to perform a few ticks of collection until it's freed enough
bytes for the new allocations.

The 'alloc' callback handles the low level memory managements;
it should emulate the standard 'realloc' functionality, with the
ability to allocate, reallocate, and free memory depending on its
parameters.

The 'expire' callback is called before any object is freed by Clok,
it allows the user to do some cleanup and whatever else should be
done before deallocating the object.  The 'alloc' parameter is the
allocated user block.

The 'preserve' callback is called to preserve the references of
an object; it'll be called before the expiration of any objects
referenced by the one passed to the callback.  This is Clok's
way of knowing what's still in use.  If a reference isn't preserved
by this callback then Clok is free to expire it, so it's crucial that
the implementation is correct.  Within this callback the user should
call the 'ck_ref' callback on any referenced Clok allocations.  Note
that passing a non-clok allocation to any of the API functions has
undefined and likely catestrophic behavior.

The 'context' field of the config is a user parameter that'll be
passed into all the callbacks, allows for the user to maintain state
without any globals.


After creating a pool you're free to allocate memory.  Clok provides
two functions for this; the 'allocSlim' function allocates atomic
data that isn't expected to contain any references.  These allocations
have less overhead than their 'fat' counterparts; but the 'preserve'
callback will never be called for them, and they're not allowed to
be passed as the 'owner' parameter for any API function, since they
can't own references.  The 'allocFat' function allocates a non-atomic
object which is expected to hold references to other objects.  These
'fat' objects will receive a 'preserve' call before Clok is allowed
to expire any objects they are known to reference; thus giving the
object a chance to maintain the reference.  The 'owner' passed to
these functions is the initial referencer; we need an initial reference
to keep the object alive, since Clok can free any unreferenced objects.
There's no need to call 'ck_ref' to for the owner after allocating,
since this is handled by the allocator itself.  If NULL is passed as
the allocation 'owner' then the allocation is considered to be a root,
and will never expire unless the pool is freed or the 'ck_unroot'
function is used to deroot it.

    void* ck_allocSlim( ck_Pool* pool, size_t size, void* owner );
    void* ck_allocFat( ck_Pool* pool, size_t size, void* owner );

The 'ck_ref' function is how we tell Clok that one object 'owner'
has obtained a reference to another 'alloc'.  Clok references have
a time limit, so after the initial ref (after obtaining a reference)
objects should periodically call 'ck_ref' again to maintain all of their
references; see the description of the 'preserve' callback for details.
Note that references are owned by objects, so for any number of
equivalent references (pointing to the same allocation) owned by the
same object, Clok only needs to be notified of one.  This means that
local reference assignment within a single object is free.  Calling
'ck_ref' with NULL as the 'owner' will promote the object to a root;
preventing it from being freed unless the pool itself is freed or
the 'ck_unroot' function is used to deroot it.

    void ck_ref( ck_Pool* pool, void* alloc, void* owner );

The 'ck_unroot' function can be called on root allocations to deroot
them.  The 'owner' parameter should be the new initial referencer;
a referencing object to keep the newly derooted object from being
collected immediately.

    void ck_unroot( ck_Pool* pool, void* alloc, void* owner );

Clok provides three API functions for invoking collection at different
levels of granularity.  The 'ck_cycle' function performs the most work,
and cycles through all the events (preservations/expirations) currently
scheduled at call time.  This function is likely too collect all
current garbage; though a few allocations may escape.  The next most
intence collection invokation is 'ck_tick'; which advances the GC clock
by one unit.  This performs all the events scheduled for the current
unit, then advances by one.  There are exactly 255 ticks in a cycle;
however depending on event scheduling some ticks will take longer than
others to complete; though there should be some level of consistency.
The 'ck_step' invocation performs an almost insignificant amount of
work with almost no delay.  The invokation either calls an expiration
callback, calls a preservation callback, or increments the clock.  The
delay for either callback depends on the user; but should be kept
minimal.  The delay for an increment is just in int addition.

    void ck_cycle( ck_Pool* pool );
    void ck_tick( ck_Pool* pool );
    void ck_step( ck_Pool* pool );

Clok also provides a more general 'ck_reserve' function; this
will perform some number of ticks until either a full cycle has
been advanced or the amount of available quota memory is greater
than the specified amount.

    void ck_reserve( ck_Pool* pool, size_t amount );

The 'ck_avail' and 'ck_used' functions are accessors for the amount
of quota room left and the amount of currently allocated memory.

    void   ck_reserve( ck_Pool* pool, size_t amount );
    size_t ck_avail( ck_Pool* pool );

Once an allocated pool is no longer needed a call to 'ck_freePool'
will deallocate the pool itself and all the allocations it manages;
so if any of its objects are still in use the pool shouldn't be freed.

    void ck_freePool( ck_Pool* pool );


## Note
This project was mostly meant as a quick expirment, and I couldn't
find the time to make sure everything works correcly.  So small as
the project is, it's likely to be pretty buggy.  Only minimal
correctness testing has been done; I'm particularly unsure about
the encoding of object size, since I used a compressed size
representation whose encoding/decoding correctness hasn't been tested
in the slightest.
