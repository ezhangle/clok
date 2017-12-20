# Clok: An Unconventional Garbage Collector
An unconventional garbage collector based on the idea of expiring
timed  references. The basic idea is the basic idea is that an object
reference lasts only a certain period of 'time' (gc ticks) after which
the object will expire if the reference isn't refreshed.  The idea
that a user need contstantly refresh references seems kinda' obserd at
first, but if you designate that responsibility to a callback which'll
be called by the GC itself at appropriate intervals; the machanism
works out alright.

## Advantages Over Traditional GC
Most modern garbage collectors use some form of tracing garbage
collection which presents the issue of pause time, which for non-
-incremental collectors can be pretty significant.  While incremental
tracing collectors can reduce the pause time to insignificant
periods, most strategies applied get more expensive as the pause
time is reduced; and incremental strategies themselves are generally
pretty complex.  The advantage of Clok is flexibility, the user can
decide how much collection should be done and when, the user is given
fine grained control over how much time can be spared for collection.
The smallest unit of Clok garbage collection is a 'step'; which 
consists of either a preservation event, and expiration event, or an
increment of the tick counter (clock).  So the pause of the smallest
Clok unit is upper bounded by the delay of two function calls, two
branches, and the callback delay, which for a preservation callback
should scan the object for references.  I believe this fine grained
control over GC could make Clok a pretty feasible alternative to
traditional tracing GC systems.  Clok's strategy is also extremely
simple to implement, the whole implementation being less than 1000
lines, excluding comments.

## Advantages Over Traditional RC
Reference Counting is a popular alternative to tracing GC when a
smaller footprint or smaller average pause time is desired.  But RC
also has some significant disadvantages, some of the most prominent
of which are:

### Bad with cycles
Reference counting doesn't handle cycles well, in fact reference
counting on its own doesn't handle cycles at all; so if cycles
need to be collected in an RC system an additional cycle detection
mechanism needs to be implemented, which can often counter some of
the initial benefits of using RC in the first place.

### Bad with concurrency
In multithreaded environments concurrency becomes an issue for ref
counting, since every increment/decrement must be atomic which can
become really expensive.  There are strategies to counter this, but
these add aditional complexity and overhead to the system.

### Polutes memory caches
In RC systems garbage collection occurs on the spot, ref counts
are decremented immediately as references are lost and when they
reach 0... a bunch of other references are deremented and a long
chain of unrelated memory might be accessed.  This random memory
access often leads to horrible polution of memory caches,
significantly reducing program performance.

### Inefficient increments/decrements
In RC systems an increment, decrement, or both occurs at every
pointer assignment.  This can lead to significant overhead,
especially in situations where such assignemnts occur fairly
frequently.  Though the cost of such a simple operation might
be small in comparison to the write barrier of some incremental
GCs, invokation of most such barriers need only occur once per
object regardless of the number of operations; ref counts are
modified for every assignment, so the cost piles up.

Clok handles cycles fairly well, though somewhat inefficiently,
every object maintains a cycle countdown which is reset when
an object's owner changes.  So cycle detection occurs on an object
only if it's gone some number of preservations without changing
ownership; which is generally only likely to occur in a cycle. When
the countdown reaches zero Clok checks if the object is in a cycle,
and if it is then 'orphans' all the members of the cycle by disabling
their preservation events.  If any of these now 'orphans' are then
referenced before they expire then they'll be 'deorphaned' and all
their referenced orphans will also be 'deorphaned'; thus preventing
expiration of referenced cycles.
Clok's system could also be fairly decent for multithreaded
applications, though not implemented yet.  Syncronization could also
occur at any level of granularity; the GC could stop the world while
it collects a full cycle, a tick, or a single step.  Alternatively
every object could have its own lock which could be obtained before
every preservation. Any of these systems could be implemented more
efficiently than RC atomic operations.
The user also has control over cache polution; if the GC is called
fairly frequently to do little work at a time then negative effects
from cache polution are more likely.  But if called less frequently
to do more work then these effects can be negligible.  Clok events
also aren't chained, so the effects of polution are less potent than
for RC in the first place.
Clok does have a fairly expensive write barrier, or more specifically
a ref barrier.  But this only needs to be called when an object obtains
a reference; localized reference assignments can occur freely and
objects never need to be unreferenced.  So in situations where local
assignments are prominent (a lot of situations) Clok's ref barrier is
likely much cheaper than the occumulative RC manipulations.

## Features
    - Fine grained control of GC delay
    - Cycle detection
    - Simple implementatio
    - Platform independent
