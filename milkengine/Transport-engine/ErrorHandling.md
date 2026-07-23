# Some notes on error handling in C/C++/Python libraries

# C

- Use goto's to a dealloc block upon error to deallocate what has been allocated
- Return an error code
- Error codes should be negative
- Hybrid C/C++ enum magic can and should be used

## C++

- Use tl::expected -- but how is that elegant on void returning functions ? Property returning functions ?


## Through nanobind

- Never propagate expected, just throw
- How should try / catch be used really ? How to not cause bloat ?

## Wrapping C++ into C

- Turn the tl::unexpected into error codes

## Calli
