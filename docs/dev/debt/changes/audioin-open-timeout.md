# libSceAudioIn

## add Functional

- [openDevice](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - capture opens are serialized. Each waits up to 3 s for SDL without holding the port table lock. Calls on the reserved slot return `0x80260109` until the open finishes. A timed-out default open supplies silence; a named open returns `0x8026010F`. Later opens use the same fallback while the abandoned worker is still running, including while it closes a device acquired late. Once it finishes, the next open retries SDL. The 3 s limit is a host choice. A PipeWire capture open can stay blocked until process exit on a host whose only source is an output monitor (issue #26458).
