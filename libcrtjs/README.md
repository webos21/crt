# libcrtjs

JavaScript/application scripting upper runtime layer.

The first target is QuickJS, used as a small C-oriented pressure test for
event-loop integration, timers, module loading, native bindings, filesystem
access, process behavior, and dynamic loading. V8 remains a later target after
the C++ runtime, JIT/code-memory policy, atomics, threading, and signal/exception
story are stronger.
