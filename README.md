# Eager Coroutine Leak

[Blog post with more details](https://ibob.bg/blog/2024/10/06/coro-throw/)

The setup is that we throw an exception from an eager coroutine before its first ever suspension point.

In an ideal world for any synchronous coroutine in its promise type we should be able to simply write:

```cpp
void unhandled_exception() {
    throw;
}
```

According to https://cplusplus.github.io/CWG/issues/2451.html it should be safe to rethrow in `uncaught_exception`. 

Unfortunately not all widely used compiles implement this. More importantly however, in the ammendment above it's not specified who frees the state buffer of the coroutine. Now, this is reflected in https://github.com/cplusplus/CWG/issues/575 (and internal links within) but a resolution is not part of the standard, much less implemented in any compiler.

What's worse is that compilers that are in wide use have an absolutely crazy (though techincally conforming) variety of how they deal with coroutine exceptions and the lifetime of the corotune state buffer.

It should be noted that gcc trunk (as of this writing the latest stable is 14.2) and clang 17 and above, have come up with their own resolution of freeing the state buffer and they don't leak it. Thus on these compilers the "ideal" solution works as desired.

Unfortunately most of the world is not on the latest compilers (especially not on bleeding edge unstable gcc). With Apple's speed of adoption, by the time Apple clang reaches 17, humanity will have colonized Mars.

So... until then what can we do with eager coroutines that throw before suspending? I'll use this repo to try to find a practical solution which works on popular compilers.

The task is: 

* Have an eager coroutine which can:
    * throw before the first suspension point
    * throw after the first suspension point
    * not throw at all
* Have this work on:
    * msvc latest
    * gcc 11.4
    * clang 15
* ... with no leaks and crashes

Ew.
