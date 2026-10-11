# Coroutine exceptions on Windows Clang

The workaround introduced in commit `151c52d` replaced
`std::rethrow_exception` with a generic exception and also removed access to
`e.what()` from the catches in `Server::run()`.

The original catch pattern triggers a Windows Clang compiler defect:
[LLVM #182584](https://github.com/llvm/llvm-project/issues/182584). A named
caught exception reference inside a coroutine loop can be moved into the heap
coroutine frame. Windows exception handling requires the catch binding to be
on the stack. Reading the resulting invalid reference, such as `e.what()`, can
crash. A potentially throwing operation before reading the reference, including
the original `std::cout` logging, triggers the faulty lowering.

This was reproduced with this repository's `Task<void>`, Clang 19.1.7 targeting
`x86_64-pc-windows-msvc`, MSVC's static runtime, and Wine 10.0:

- A loop containing `co_await` and a named catch that prints `error.what()`
  crashes on its first iteration.
- The same catch outside a loop succeeds.
- A loop with an unnamed catch that calls an ordinary function to inspect the
  current exception succeeds and preserves the original exception message.
- Replacing the original rethrow with the generic exception still crashes in
  the named catch loop.

Compiling the historical `Server::run()` from `151c52d^` produces the same
invalid catch bindings. Generated LLVM IR confirms the failure: the broken
`catchpad` points to a field inside the coroutine frame, while the working
catches use a stack allocation or no catch binding. `Server::run()` uses
unnamed catches and calls the ordinary `log_current_exception()` helper to
log the original message. The helper rethrows the active exception and binds
the caught reference on its normal stack. Keep exception inspection in ordinary
functions when working around affected Windows Clang versions.

`PromiseBase::unhandled_exception()` correctly captures the exception, and
`Task::await_resume()` should rethrow it. Replacing it with a generic exception
loses its type and details and does not address the compiler defect.

The upstream fix is
[LLVM #186728](https://github.com/llvm/llvm-project/pull/186728), which keeps
catch bindings on the stack. It was merged on March 25, 2026. Use a compiler
containing that fix before introducing named catch references in coroutine
loops; a newer version number alone does not guarantee it contains the fix.
