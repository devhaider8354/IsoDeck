Place the official Ventoy Windows package here, renamed exactly:

    third_party/ventoy.zip      (from  ventoy-x.y.z-windows.zip)

Download: https://github.com/ventoy/Ventoy/releases
Verify the SHA-256 published on the release page before building.

The zip is embedded as-is into IsoDeck.exe (resource IDR_VENTOY_ZIP) and extracted at runtime to
%LOCALAPPDATA%\IsoDeck\engine\ on first use. Ventoy is licensed under GPLv3 - see LICENSE.
