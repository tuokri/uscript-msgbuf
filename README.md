# uscript-msgbuf

Lightweight messaging protocol code generator for UnrealScript and C++.
The project goal is to generate bandwidth-optimized message handling code,
where the messages take as few bytes as possible.

## Dependencies

- [Inja](https://github.com/pantor/inja)
- [JSON for Modern C++ (nlohmann/json)](https://github.com/nlohmann/json)
- [Boost (Program Options)](https://www.boost.org/doc/libs/1_83_0/doc/html/program_options.html)
    - Tested with Boost versions 1.82 and 1.83.

Install using vcpkg:

```shell
vcpkg install inja nlohmann-json boost-program-options
```
