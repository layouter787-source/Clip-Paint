plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.clippaint"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.clippaint"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-O3")
            }
        }
    }

    buildTypes {
        getByName("debug") {
            // Install the feature build alongside the stable/main app.
            // This avoids silently opening the old app when debug signatures differ.
            applicationIdSuffix = ".feature"
            versionNameSuffix = "-feature"
        }
    }

    buildFeatures {
        buildConfig = true
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}
