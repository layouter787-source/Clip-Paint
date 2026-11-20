# Clip Paint

Native Android drawing app project.

## Build

Every push to `main` or `master`, pull request, and manual workflow dispatch runs the Android GitHub Actions build.

The workflow uses JDK 17 and Gradle 8.11.1 and publishes the debug APK as the `clip-paint-debug` workflow artifact.

## Current repository state

The repository was empty when work began. This commit establishes the Android Gradle project and a buildable starter drawing surface with brush, eraser, undo, and clear controls.

The supplied CodeAssist `.caproj` package contains a larger Kotlin/C++ drawing engine, brush libraries, selection tools, and project-management screens. That package has not yet been imported into this Git repository; therefore this starter must not be described as the complete Clip Paint feature set. The native engine and the rest of the supplied app need to be migrated into the Gradle module before the full application is ready.

## Package

- Application ID: `com.clippaint`
- Minimum Android: API 26
- Target Android: API 35
