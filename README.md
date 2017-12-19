# Clok: An Unconventional Garbage Collector
Clok is a small garbage collector designed around
a somewhat unconventional mechanism.  Whereas most
modern collectors use either reference counting or
generational sweeping collectors; Clok is designed
around the idea of expiring (or timed) references.
An object indicates to the GC that it's obtained
a reference to another by calling a 'ref' function;
this'll keep the object alive for some number of
'ticks' after which the owner is required to re-ref
the object.  If an object owner doesn't re-ref before
the expiration time then it's assumed that the object
can be disposed.  If the object is re-refed then the
expiration time is extended.  The GC ensures that an
object will remain alive until the last reference owner
has a chance to re-ref it; this is done via a 'preserve'
callback which is also called at 'tick' intervals.

For most use cases it seems that conventional GC's
performance would surpass that of Clok; however there
are some use cases where this system may have some
potential... but I'm kinda' a novice, so it may also
be that the system is impractical.

# Advantages of Clok Over GC
As far as performance goes sweeping GCs are hard to beat,
but they have a major drawback: pause time.  Sweeping GCs
tend to scan for garbage in bulk; often having to scan
the entire object pool before they can decide which objects
to dispose of.  While this is acceptable in most applications,
there are some cases where it causes problems, especially
in realtime control or multimedia systems.  Clok, while less
efficient, gives the user direct control of collection pace;
the API allows for either a 'tick' which, on average, scans
1/255th of all objects; or a 'step' which executes a single
event (either expire or preserve).  This flexibility may give
Clok some practical applications... but whether or not the
efficiency cost is too high for the trade remains to be seen...
I haven't run any benchmarks yet.

# Advantages of RC Over GC
The main advantage of RC over GC, as far as I know, is reduced
pause time.  RC's pause time is reduced to the handling of a single
reference at once.  This is beneficial for such applications as
mentioned above which don't tolerate extensive pauses.  The cost of this
reduced pause time is performance and reference cycles.  With the
accumulative cost of reference manipulation, cache polution, and
reference concurrency... the performance of reference counting isn't
very impressive when compared to GC; and even less so in cases where
localized assignements occur fairly frequently.  RC also suffers from
the issue of reference cycles which require an additional collector
to dispose of.

# Advantages of Clok Over RC
While RC's performance isn't very impressive compared to GC... Clok's
performance is expected to be event worse in most cases.  However there
are some istances where Clok may hold an advantage.  Since Clok references
belong to objects and are preserved by the owning object regardless of count,
local assignments don't require re-referencing.  So for instance a VM stack
can function independently without updating any ref-counts.  In such instances
where local assignments occur frequently the... this can severly impact the
performance of RC; but doesn't cost Clok anything.  Another advantage of Clok
is the user's control of when collection happens; cache pollution can thus
be avoided and concurrency is more manageable.  The frequent 'preserve' events
also give Clok an entry point to track down reference cycles; but this puts a
dent in performance.

# Disadvantages of Clok
The disadvantages of Clok seem fairly obvious; it isn't very efficient.  The
cost of referencing an object is far more expensive than it is in RC; and 
the frequent 'preserve' events result in a scanning redundancy that GC
doesn't have.  Whether the performance cost of Clok is too great to make
the system feasable... remains to be seen; will need to run some benchmarks.
