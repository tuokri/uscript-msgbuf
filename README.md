# UScript-MsgBuf

UScript-MsgBuf (UMB) is a lightweight messaging protocol code generator
for UnrealScript and C++. Think Protobuf, but specialized for a single
application domain. The project goal is to generate bandwidth-optimized
message handling code, where the messages take as few bytes as possible,
while keeping UnrealScript restrictions in mind. UnrealScript performance
is the priority concern. Network packets are built to fit in 255 bytes,
which is the limit imposed by UnrealScript's `TcpLink` class, specifically
the `SendBinary` function.

## Building

### Dependencies

- [Inja](https://github.com/pantor/inja)
- [Boost](https://www.boost.org/)
    - Tested with Boost versions 1.82 and 1.83.
- [doctest](https://github.com/doctest/doctest) (for testing)
- icu (TODO: DOCUMENT ME)

Install using vcpkg:

```shell
vcpkg install inja boost-program-options \
  boost-filesystem boost-algorithm boost-process boost-dll doctest icu
```

### TODO:

- Probably have to force some more dependencies since unit test messages
  are sometimes not regenerated even though the templates or generator changes.

- Allow setting default values for fields?
