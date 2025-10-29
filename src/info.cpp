#include "info.hpp"
#include "audio.hpp"
#include "log.hpp"
#include "string.hpp"

#include <mimalloc.h>
#include <unity.h>
#include <glm/detail/setup.hpp>
#include <tinycthread.h>

#include <external/glad.h>
#include <external/glfw/include/GLFW/glfw3.h>

#ifdef call_once
#undef call_once
#endif

void static i_print_ouro(VersionInfo *info) {
    lld("ouro: %s (%s)", info->c, info->build_type);
}

// Windows.
#ifdef _WIN32
#include <windows.h>
void static i_print_os() {
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (GetVersionEx((OSVERSIONINFO *)&osvi)) {
        lld("OS: Windows %lu.%lu (Build %lu)", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    } else {
        lld("OS: Windows (unknown version)");
    }
}

// Linux.
#elif __linux__
#include <sys/utsname.h>
void static i_print_os() {
    struct utsname uts = {};
    if (uname(&uts) == 0) {
        lld("OS: %s %s %s %s %s", uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);
    } else {
        lld("OS: Linux (unknown version)");
    }
}

// Unknown.
#else
void static i_print_os() {
    lld("OS: UNKNOWN");
}
#endif

void static i_print_raylib() {
    lld("raylib: %s", RAYLIB_VERSION);
}

void static i_print_glfw() {
    S32 glfw_major_version = 0;
    S32 glfw_minor_version = 0;
    S32 glfw_revision      = 0;
    glfwGetVersion(&glfw_major_version, &glfw_minor_version, &glfw_revision);
    lld("GLFW: %d.%d.%d", glfw_major_version, glfw_minor_version, glfw_revision);
}

void static i_print_glad() {
    lld("glad: %s", GLAD_GENERATOR_VERSION);
}

void static i_print_opengl() {
    lld("OpenGL: %s", glGetString(GL_VERSION));
}

void static i_print_glsl() {
    C8 const *glsl_version = (C8 const *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    lld("GLSL: %s", glsl_version ? glsl_version : "Unknown");
}

void static i_print_renderer() {
    C8 const *renderer = (C8 const *)glGetString(GL_RENDERER);
    C8 const *vendor   = (C8 const *)glGetString(GL_VENDOR);
    lld("Renderer: %s", renderer ? renderer : "Unknown");
    lld("Vendor: %s", vendor ? vendor : "Unknown");
}

void static i_print_monitor() {
    S32 const monitor = GetCurrentMonitor();
    lld("Monitor: %s", GetMonitorName(monitor));
    lld("Resolution: %dx%d", GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    lld("RefreshRate: %d Hz", GetMonitorRefreshRate(monitor));
}

void static i_print_gamepad() {
    for (S32 i = 0; i < 4; i++) {
        if (IsGamepadAvailable(i)) { lld("Gamepad %d: %s", i, GetGamepadName(i)); }
    }
}

void static i_print_fmod() {
    U32 const fmod_version       = FMOD_VERSION;
    U32 const fmod_patch_version = (fmod_version & 0x0000FF00) >> 8;
    U32 const fmod_minor_version = (fmod_version & 0x000000FF);
    U32 const fmod_major_version = (fmod_version & 0xFFFF0000) >> 16;
    lld("FMOD: %d.%d.%d", fmod_major_version, fmod_minor_version, fmod_patch_version);
}

void static i_print_tinycthread() {
    lld("TinyCThread: %d.%d", TINYCTHREAD_VERSION_MAJOR, TINYCTHREAD_VERSION_MINOR);
}

void static i_print_unity() {
    lld("Unity: %d.%d.%d", UNITY_VERSION_MAJOR, UNITY_VERSION_MINOR, UNITY_VERSION_BUILD);
}

void static i_print_glm() {
    lld("GLM: %d.%d.%d", GLM_VERSION_MAJOR, GLM_VERSION_MINOR, GLM_VERSION_PATCH);
}

void static i_print_mimalloc() {
    S32 const version = mi_version();
    S32 const major   = version / 100;
    S32 const minor   = (version % 100) / 10;
    lld("mimalloc: %d.%d", major, minor);
}

void static i_print_simd() {
    lld("SIMD Support:");

#ifdef GLM_FORCE_SSE2
    lld("SSE2: Enabled");
#else
    lld("SSE2: Disabled");
#endif

#ifdef GLM_FORCE_SSE3
    lld("SSE3: Enabled");
#else
    lld("SSE3: Disabled");
#endif

#ifdef GLM_FORCE_SSE41
    lld("SSE4.1: Enabled");
#else
    lld("SSE4.1: Disabled");
#endif

#ifdef GLM_FORCE_SSE42
    lld("SSE4.2: Enabled");
#else
    lld("SSE4.2: Disabled");
#endif

#ifdef GLM_FORCE_AVX
    lld("AVX: Enabled");
#else
    lld("AVX: Disabled");
#endif

#ifdef GLM_FORCE_AVX2
    lld("AVX2: Enabled");
#else
    lld("AVX2: Disabled");
#endif

#ifdef GLM_FORCE_INTRINSICS
    lld("SIMD Intrinsics: Enabled");
#else
    lld("SIMD Intrinsics: Diosabled");
#endif
}

void static i_print_compiler() {
#ifdef __clang__
    lld("Compiler: Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    lld("Compiler: GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    lld("Compiler: MSVC %d", _MSC_VER);
#else
    lld("Compiler: Unknown");
#endif

#ifdef __cplusplus
    C8 const *cpp_version = "Unknown";
         if (__cplusplus >= 202302L) { cpp_version = "C++23"; }  // NOLINT
    else if (__cplusplus >= 202002L) { cpp_version = "C++20"; }  // NOLINT
    else if (__cplusplus >= 201703L) { cpp_version = "C++17"; }  // NOLINT
    else if (__cplusplus >= 201402L) { cpp_version = "C++14"; }  // NOLINT
    else if (__cplusplus >= 201103L) { cpp_version = "C++11"; }  // NOLINT
    else if (__cplusplus >= 199711L) { cpp_version = "C++98"; }  // NOLINT
    lld("C++ Standard: %s", cpp_version);
#endif
}

void static i_print_build_info() {
    lld("Built: " __DATE__ " " __TIME__);

#ifdef OURO_DEBUG
    lld("Configuration: Debug");
#elif defined(OURO_RELEASE)
    lld("Configuration: Release");
#elif defined(OURO_DEVEL)
    lld("Configuration: Development");
#else
    lld("Configuration: Unknown");
#endif
}

void static i_print_audio() {
    FMOD::System *system = audio_get_fmod_system();
    if (system) {
        S32 driver_count         = 0;
        FMOD_RESULT const result = system->getNumDrivers(&driver_count);
        if (result == FMOD_OK && driver_count > 0) {
            lld("Audio: FMOD initialized with %d drivers", driver_count);
        } else {
            llw("Audio: FMOD initialized but no drivers available");
        }
    } else {
        llw("Audio: FMOD system not initialized");
    }
}

VersionInfo info_create_version_info(U8 major, U8 minor, U8 patch, C8 const *build_type) {
    VersionInfo version_info = {};
    version_info.major      = major;
    version_info.minor      = minor;
    version_info.patch      = patch;
    version_info.c          = PS("%d.%d.%d %s", major, minor, patch, build_type)->c;
    version_info.build_type = build_type;
    return version_info;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

SZ info_get_cpu_core_count() {
    SZ cpu_count = 1;
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    cpu_count = (SZ)sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__) || defined(__linux__)
    cpu_count = (SZ)sysconf(_SC_NPROCESSORS_ONLN);
#else
    llw("Unknown platform, defaulting to 1 CPU core");
#endif
    lld("Number of CPU cores: %zu", cpu_count);
    return cpu_count;
}

void info_print_system(VersionInfo *info) {
    lld("System Information");
    i_print_ouro(info);
    i_print_build_info();
    i_print_compiler();
    i_print_os();
    i_print_raylib();
    i_print_glfw();
    i_print_glad();
    i_print_opengl();
    i_print_glsl();
    i_print_renderer();
    i_print_monitor();
    i_print_gamepad();
    i_print_fmod();
    i_print_audio();
    i_print_tinycthread();
    i_print_unity();
    i_print_glm();
    i_print_mimalloc();
    i_print_simd();
}
