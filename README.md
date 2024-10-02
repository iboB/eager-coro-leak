# Eager Coroutine Leak

The setup is that we throw an exception from an eager coroutine before its first ever suspension point.

According to https://cplusplus.github.io/CWG/issues/2451.html it should be safe to rethrow in `uncaught_exception`. First, not all widely used compiles implement this, but more importantly even there it's not specified who frees the state buffer of the coroutine.

This is reflected in https://github.com/cplusplus/CWG/issues/575 (and internal links within) but a resolution is not part of the standard, much less implemented in any compiler.

What's worse is that compilers that are in wide use have an absolutely crazy (though techincally conforming) variety of how they deal with coroutine exceptions and the lifetime of the corotune state buffer.

So... until then what can we do with eager coroutines that throw before suspending? I'll use this repo to try find a practical solution which works on modern compilers.
