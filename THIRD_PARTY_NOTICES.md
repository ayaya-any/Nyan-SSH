# Third-party notices

Nyan SSH is built on the following open-source components.

## libssh2 1.11.1

- Project: https://www.libssh2.org/
- License: BSD 3-Clause
- Usage: fetched by CMake FetchContent (version 1.11.1, SHA256 pinned) and
  statically linked. SSH cryptography is provided by OpenSSL 3.x.
- Full text: `licenses/libssh2-COPYING`.

## Qt 6.12

- Project: https://www.qt.io/
- License: GNU Lesser General Public License v3 (LGPL-3.0) / commercial.
- Usage: Qt Core, Gui, Widgets, Svg and Test are dynamically linked. The Qt
  runtime DLLs are deployed adjacent to the executable, which keeps them
  replaceable as required by the LGPL.
- Qt is not modified. Qt source code is available from https://download.qt.io/.
- License text: `licenses/Qt-LICENSE`.

## Material Icons

- Project: https://github.com/google/material-design-icons
- License: Apache License 2.0.
- Usage: the bundled `MaterialIcons-Regular.ttf` is embedded in the application
  resources and used as the primary UI icon font. Original SVG artwork remains
  as a fallback for the logo and custom/decorative graphics.
- License text: `licenses/MaterialIcons-LICENSE.txt`.

## OpenSSL 3.x

- Project: https://www.openssl.org/
- License: Apache License 2.0.
- Usage: the portable cryptography backend for libssh2. On Windows the
  `libcrypto-3-x64.dll` runtime is deployed next to the executable.
- License text: `licenses/OpenSSL-LICENSE.txt`.

## Windows SDK

- DPAPI (`CryptProtectData`/`CryptUnprotectData`) is used for local credential
  encryption. Header and import libraries come from the Microsoft Windows SDK.

The Material-style widget kit, terminal emulator, SFTP manager and application
code are original work and licensed under the MIT license (see `LICENSE`).
