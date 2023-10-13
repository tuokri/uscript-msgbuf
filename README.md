# uscript-msgbuf

Lightweight messaging protocol code generator for UnrealScript and C++.
The project goal is to generate bandwidth-optimized message handling code,
where the messages take as few bytes as possible.

## Dependencies

- [Inja](https://github.com/pantor/inja)
- [Boost (Program Options, Filesystem)](https://www.boost.org/)
    - Tested with Boost versions 1.82 and 1.83.

Install using vcpkg:

```shell
vcpkg install inja boost-program-options boost-filesystem
```
