# Sink Hot-Reload Implementation - Phase 1

## Problem Solved

Previously, when users edited sink configurations through the browser UI, the changes were only reflected in the UI but did not affect the actual runtime behavior. The publishers (NNG and OSC) were only configured once at startup and never updated when configuration changed.

## Phase 1 Solution

### Changes Made

1. **Updated RestApi class** ([src/io/rest_handlers.h](src/io/rest_handlers.h), [src/io/rest_handlers.cpp](src/io/rest_handlers.cpp)):
   - Added `OscPublisher&` reference to constructor and member variables
   - Implemented `applySinksRuntime()` method that:
     - Stops current NNG and OSC publishers
     - Finds active sinks from config (one per type for Phase 1)
     - Starts publishers with the active sink configurations
     - Logs which endpoints are applied to runtime

2. **Added hot-reload triggers**:
   - `postSink()` - calls `applySinksRuntime()` after adding new sink
   - `patchSink()` - calls `applySinksRuntime()` after editing sink
   - `deleteSink()` - calls `applySinksRuntime()` after deleting sink
   - `postConfigsLoad()` - calls `applySinksRuntime()` after loading config file
   - `postConfigsImport()` - calls `applySinksRuntime()` after importing config

3. **Updated main.cpp** ([src/main.cpp](src/main.cpp)):
   - Removed per-sink startup loop (lines 31-38)
   - Pass `OscPublisher` reference to `RestApi` constructor
   - Call `rest->applySinksRuntime()` once after controllers are registered

### Runtime Behavior

- **Before**: Sink edits only updated UI, runtime publishers unchanged
- **After**: Sink edits immediately reconfigure runtime publishers with detailed logging

### Logging Output

When sinks are applied to runtime, you'll see logs like:
```
[RestApi] Applying sink configuration to runtime...
[RestApi] Applied NNG sink: tcp://0.0.0.0:5555 (topic: clusters, rate_limit: 120Hz)
[RestApi] Applied OSC sink: osc://192.168.1.100:7000/hokuyohub/cluster (topic: lidar_data, rate_limit: 30Hz)
[RestApi] Sink runtime configuration complete
```

## Phase 1 Limitations

1. **One sink per type**: Only the first NNG sink and first OSC sink in the configuration will be active at runtime. If multiple sinks of the same type exist, warnings are logged but only the first is used.

2. **Topic field unused**: The `topic` field is stored and displayed in the UI but not yet used by the publishers at runtime.

3. **No concurrent multi-sink support**: Cannot have multiple NNG endpoints or multiple OSC endpoints running simultaneously.

## Testing

To test the hot-reload functionality:

1. Start the server
2. Use the browser UI to add/edit/delete sinks
3. Check server logs for runtime application messages
4. Verify that external clients connecting to the endpoints see the changes

## Next Steps (Phase 2)

- Design and implement `PublisherManager` for true multi-sink support
- Support multiple concurrent NNG and OSC publishers
- Utilize the `topic` field in publisher routing
- Add comprehensive test coverage in `scripts/test_rest_api.sh`