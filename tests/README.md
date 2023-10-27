### TESTING TODOs

- Add tests that read individual .jinja files and generate
  a bunch of variants using test-defined data.

- Add messages to test data that don't have any dynamic fields
  but exceed the packet limit to make sure they are correctly
  flagged as dynamic messages.

- Test each encoding and decoding function individually.

- Set up a test that mutates every field of a message and checks results each time.
    - Fuzzing? To supplement the test?
