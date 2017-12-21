# Clok: An Unconventional Garbage Collector
An unconventional garbage collector based on the idea of expiring
timed  references. The basic idea is the basic idea is that an object
reference lasts only a certain period of 'time' (gc ticks) after which
the object will expire if the reference isn't refreshed.  The idea
that a user need contstantly refresh references seems kinda' obsurd at
first, but if the task is designated to a callback which is called by
the collector itself; this is more reasonable, the callback is really
just a generic representation for a reference scanner.
