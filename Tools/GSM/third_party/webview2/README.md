# WebView2 SDK subset

Microsoft.Web.WebView2 **1.0.2651.64**, from the existing project package at `Plugins/BehaviorU/BehaviorULauncher/packages/microsoft.web.webview2/1.0.2651.64`.

Upstream: https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.2651.64

Contains the unchanged `build/native/include/WebView2.h`, x64 `build/native/x64/WebView2LoaderStatic.lib`, and package `LICENSE.txt`. The loader is statically linked; end users need the Microsoft Edge WebView2 Runtime installed separately. No separate loader DLL is required. Builds use this vendored subset offline.
