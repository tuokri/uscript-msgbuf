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

### Supported compilers

Tested on the following compilers. UMB uses C++23 features, so building
could also work on older compilers, depending on their C++23 support status.

- GCC 14.0.1
- MSVC 19.40.33811

*Clang is currently unsupported due to Inja compilation issues.*

### Dependencies

- [Inja](https://github.com/pantor/inja)
- [Boost](https://www.boost.org/)
    - Tested with Boost versions 1.82 and 1.83.
- [doctest](https://github.com/doctest/doctest) (for testing)
- ICU (TODO: DOCUMENT ME)

Install using vcpkg:

```shell
vcpkg install inja boost doctest icu
```

## TODO:

- ADD VCPKG AS A SUBMODULE FOR SELF-CONTAINED DEPENDENCY MANAGEMENT

- Improve vcpkg dependency handling.
    - Fork vcpkg?
    - Pin version in manifest (vckpg.json)?

- Probably have to force some more dependencies since unit test messages
  are sometimes not regenerated even though the templates or generator changes.

- Allow setting default values for message fields?

- Add pre-build checker script that warns if build directory has old templates.
    - This can happen if templates are removed and build directory is not manually
      cleaned before doing a new build, causing potential failures or minor issues.

- Consider declaring fields present in a packet with a 32 bit mask?
    - Add another package type in addition to current one?
        - Would not make sense for small messages if the mask overhead
          is large relative to the packet size.
    - Would limit the maximum number of fields in a message to 32.
    - Would allow only sending the fields that are actually set in the message.
    - Increases header overhead by 4 bytes.
    - Increased complexity for little gain? How would this work with static
      message coding in UnrealScript? Requires keeping track of set and unset
      fields, making indexing impossible at template generation time?

- If the generator executable is changed and compiled, all templates should be
  invalidated. Is there a smart way to do this with CMake?

- Investigate floating point edge cases. C++ inf/nan <-> UScript inf/nan?

## Possible future major refactor/rewrite points:

- Get rid of abstract base class Message, use std::variant instead.
    - All metadata regarding messages (data types, layout) is known at
      code generation time, abstract base class may not be necessary.
    - Better performance (although a low priority since the major bottleneck
      is the UnrealScript counterpart.)

- Make multipart messages to_bytes() write out data that is ready to be
  sent to a socket (bake headers in the serialized format). Update from_bytes()
  similarly. This should make sending out multipart messages much simpler, while
  making serialization/deserialization code a bit more complex. This data format
  is also much more coherent (IMO).

# License

```
Copyright (C) 2023-2024  Tuomo Kriikkula
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```
