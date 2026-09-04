![Firefox Browser](./docs/readme/readme-banner.svg)

This source tree is based on `FIREFOX_155_0_RELEASE` at commit `21a0961191033207dc167b842f6c251f337b0e54`.

### Build

Build locally for the host platform:

```sh
DEVELOPER_OPTIONS=1 CARGO_BUILD_JOBS=1 ./mach build -j6
```

Package the host build:

```sh
DEVELOPER_OPTIONS=1 CARGO_BUILD_JOBS=1 ./mach package
```

For a Linux-hosted Windows x86_64 cross-build, fetch the Mozilla CI toolchains:

```sh
mkdir -p artifacts/win64-mingw-fetches
MOZ_FETCHES_DIR="$PWD/artifacts/win64-mingw-fetches" ./mach artifact toolchain --from-build \
  mingw32-rust linux64-7zz linux64-upx linux64-wine linux64-cbindgen linux64-nasm linux64-node \
  linux64-clang-mingw-x64 linux64-mingw32-nsis linux64-mingw-fxc2-x86 linux64-dump_syms \
  sysroot-x86_64-linux-gnu sysroot-wasm32-wasi winappsdk-x86_64-pc-windows-msvc
```

If the toolchain fetch places directories in the repository root, move `7zz`, `cbindgen`, `clang`, `dump_syms`, `fxc2`, `nasm`, `node`, `nsis`, `rustc`, `sysroot-wasm32-wasi`, `sysroot-x86_64-linux-gnu`, `upx`, `winappsdk-x86_64-pc-windows-msvc`, and `wine` into `artifacts/win64-mingw-fetches/`.

Fetch the Windows Rust crate used by the Windows App SDK bindings:

```sh
mkdir -p artifacts/win64-mingw-fetches/windows-rs
curl -L --fail --retry 3 --user-agent 'cargo/1.90.0' \
  --output artifacts/win64-mingw-fetches/windows-rs/windows-0.62.2.crate \
  https://static.crates.io/crates/windows/windows-0.62.2.crate
tar -xzf artifacts/win64-mingw-fetches/windows-rs/windows-0.62.2.crate \
  -C artifacts/win64-mingw-fetches/windows-rs
```

Create `artifacts/mozconfig-win64-mingwclang-minimal`:

```sh
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-x86_64-pc-windows-gnu
mk_add_options "export UPLOAD_PATH=@TOPSRCDIR@/artifacts/win64-mingw-crash-diagnostics"
mk_add_options "export MOZ_WINDOWS_APP_SDK_DIR=$MOZ_FETCHES_DIR/winappsdk-x86_64-pc-windows-msvc"
mk_add_options "export MOZ_WINDOWS_RS_DIR=$MOZ_FETCHES_DIR/windows-rs/windows-0.62.2"
ac_add_options --target=x86_64-pc-windows-gnu
ac_add_options --with-toolchain-prefix=x86_64-w64-mingw32-
ac_add_options --disable-warnings-as-errors
mk_add_options "export WIDL_TIME_OVERRIDE=0"
ac_add_options --disable-webrtc
ac_add_options --disable-geckodriver
ac_add_options --disable-update-agent
ac_add_options --disable-maintenance-service
ac_add_options --disable-default-browser-agent
ac_add_options --disable-notification-server
ac_add_options --disable-zucchini
HOST_CC="$MOZ_FETCHES_DIR/clang/bin/clang"
HOST_CXX="$MOZ_FETCHES_DIR/clang/bin/clang++"
CC="$MOZ_FETCHES_DIR/clang/bin/x86_64-w64-mingw32-clang"
CXX="$MOZ_FETCHES_DIR/clang/bin/x86_64-w64-mingw32-clang++"
RUSTC="$MOZ_FETCHES_DIR/rustc/bin/rustc"
CARGO="$MOZ_FETCHES_DIR/rustc/bin/cargo"
CXXFLAGS="-fms-extensions -include _mingw.h"
CFLAGS="-include _mingw.h"
mk_add_options "export PATH=$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/fxc2/bin:$MOZ_FETCHES_DIR/rustc/bin:$MOZ_FETCHES_DIR/wine/bin:$PATH"
ac_add_options --with-branding=browser/branding/unofficial
```

Then configure, build, and package:

```sh
DEVELOPER_OPTIONS=1 CARGO_BUILD_JOBS=1 \
  MOZ_FETCHES_DIR="$PWD/artifacts/win64-mingw-fetches" \
  MOZCONFIG="$PWD/artifacts/mozconfig-win64-mingwclang-minimal" \
  ./mach --log-no-times configure

DEVELOPER_OPTIONS=1 CARGO_BUILD_JOBS=1 \
  MOZ_FETCHES_DIR="$PWD/artifacts/win64-mingw-fetches" \
  MOZCONFIG="$PWD/artifacts/mozconfig-win64-mingwclang-minimal" \
  ./mach build -j6

DEVELOPER_OPTIONS=1 CARGO_BUILD_JOBS=1 \
  MOZ_FETCHES_DIR="$PWD/artifacts/win64-mingw-fetches" \
  MOZCONFIG="$PWD/artifacts/mozconfig-win64-mingwclang-minimal" \
  ./mach package
```

The runnable Windows archive is written to `obj-x86_64-pc-windows-gnu/dist/ferifox-155.0.en-US.win64.zip`. The launcher is `obj-x86_64-pc-windows-gnu/dist/ferifox/ferifox.exe` and requires the DLLs and resources from that zip.

[Firefox](https://firefox.com/) is a fast, reliable and private web browser from the non-profit [Mozilla organization](https://mozilla.org/).
