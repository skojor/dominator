# Dominator contributor guide

Dominator is a Qt Widgets/C++ application for a Sea Tel 6012-45 motorized dish. It exchanges signal readings, scan control requests, and pointing telemetry with Hauken over local IPC. Hauken is a separate project, not a build dependency.

## Windows build and tests

Run these commands in PowerShell from the repository root. Prefer the existing `build` directory: it uses the `Visual Studio 17 2022` generator, `x64`, and the Qt 6.12.0 `msvc2022_64` kit. This follows Hauken's build method, but not its Qt version. CMake and CTest may not be on `PATH`.

If `build\CMakeCache.txt` does not exist, configure once:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64' -DBUILD_TESTING=ON
```

For an existing build, inspect the cache before changing the generator or kit. Use a separate build directory for a different kit or for Qt Creator's Ninja configuration.

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --config Debug
$env:PATH = 'C:\Qt\6.12.0\msvc2022_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build -C Debug --output-on-failure
```

Run tests only after a successful build, and match the build and test configuration. Use CTest's `-R` to select a focused suite: `DominatorTests` (IPC and scan state), `DishSettingsTests`, `DishDirectionIndicatorTests`, or `DishCliClientTests`. With the same Qt runtime on `PATH`, launch the Debug application with `.\build\Debug\Dominator.exe`.

## Code map and conventions

- `MainWindow` wires together the UI and controllers. Keep protocol parsing and scan logic out of the window class, and keep network I/O asynchronous.
- `DishSettings` stores the front-page address and scan fields and the connection settings using `QSettings`. The default CLI port is `2003`. Preserve Windows DPAPI password storage; do not log or commit credentials. Telnet is unencrypted and intended for trusted networks.
- `DishCliClient` handles the TCP/Telnet login and polls read-only `show status antenna earth_az` / `earth_el` after login. The login and polling have simulated coverage; confirm the reply format and true-north reference on the live controller. The app does not send motion commands. `ScanController` tracks logical scan state, not hardware acknowledgement.
- `LocalIpcServer` and `PositionReporter` implement the [Hauken IPC contract](./IPC_PROTOCOL.md). Hauken sends dBuV readings as they arrive; Dominator sends the latest known position once per second when available. Azimuth is in degrees clockwise from true north; elevation is in degrees above the horizon. Keep local Start/Stop usable without Hauken. Losing IPC pauses an active scan; reconnecting does not resume it automatically.
- The runtime SVG is embedded via `resources\icons.qrc` and CMake `AUTORCC`. The Windows executable icon is a separate `.ico` resource.

## Hardware references and future work

- The DataStream session and buffered backend in [datastream.h](./datastream.h) are simulator-only and built by `DataStreamTests`; they are not wired into the live CLI or UI. Simulator abort and completion semantics are not firmware guarantees. See IMA CLI Rev S pp. 21 and 61: the timestamp formats listed there differ, and `end` and `clear` are not documented physical stops.

- Consult the [IMA CLI specification](./doc/135163_s-document-ima-cli-protocol-specification.pdf) for command behavior and the [6012-45 installation manual](./doc/99-161588-b-installation-manual-sea-tel-6012-45-eccn-ear99.pdf) for setup and operating limits. Confirm firmware-specific behavior on the intended controller before relying on it. Use simulated peers in automated tests; do not send commands to a live dish without explicit approval.
- The manuals do not document a custom rectangular raster command. A leg-by-leg scan using `PATH` is a candidate, not an implemented feature. Verify coordinates, leg completion, and stop/pause behavior before enabling motion.
- **Tools → Satellite pointing preview** uses the [N2YO positions API](https://www.n2yo.com/api/#positions). Starting the preview remembers the NORAD ID, latitude, longitude, and altitude and sends the observer coordinates to N2YO. Windows DPAPI encrypts the saved API key for the current Windows user; on other platforms the key is not saved. Predictions are time-limited, display-only, and never sent to the dish or Hauken. Test response handling with `N2yoClientTests` and synthetic data rather than a live key.
- Local SGP4 tracking is not implemented. Vallado's C++ SGP4 code from CelesTrak is the candidate dependency. If adopted, pin the revision, retain the upstream notice, and verify against reference vectors. Keep orbit propagation and conversion to observer-relative azimuth/elevation separate from dish commands.
