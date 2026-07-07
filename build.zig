const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const c_flags = &[_][]const u8{
        "-std=gnu99",
        "-Wall",
        "-Wextra",
        "-O2",
        "-Isrc/core",
        "-Isrc/gui",
        "-Isrc/cli",
        "-Isrc/daemons",
        "-Iprotocols",
    };

    // === Unified Binary (Zig harness) ===
    {
        const exe = b.addExecutable(.{
            .name = "ocws",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
                .root_source_file = b.path("src/ocws.zig"),
            }),
        });

        b.installArtifact(exe);

        // Tests
        const tests = b.addTest(.{
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .root_source_file = b.path("src/tests.zig"),
                .link_libc = true,
            }),
        });
        tests.root_module.addIncludePath(b.path("src"));

        const run_tests = b.addRunArtifact(tests);
        const test_step = b.step("test", "Run integration tests");
        test_step.dependOn(&run_tests.step);
    }

    const c_utils = [_][]const u8{
        "ocws-shot",
        "ocws-clip",
        "ocws-lock",
        "ocws-sysmon",
        "ocws-network-bandwidth",
        "ocws-player",
        "ocws-state",
        "ocws-validate",
        "ocws-brightness",
        "ocws-volume",
        "ocws-recorder",
        "ocws-emit",
        "ocws-search",
    };

    for (c_utils) |util_name| {
        const exe = b.addExecutable(.{
            .name = util_name,
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        const src_path = b.fmt("src/cli/{s}.c", .{util_name});

        exe.root_module.addCSourceFile(.{
            .file = b.path(src_path),
            .flags = c_flags,
        });

        b.installArtifact(exe);
    }

    // ocws-kv: links ocws-kv.c library + CLI
    {
        const exe = b.addExecutable(.{
            .name = "ocws-kv",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/core/ocws-kv.c"),
            .flags = c_flags,
        });
        exe.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-kv-cli.c"),
            .flags = c_flags,
        });

        b.installArtifact(exe);
    }

    // ocws-fonts: links ocws-fonts.c library + CLI
    {
        const exe = b.addExecutable(.{
            .name = "ocws-fonts",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/core/ocws-fonts.c"),
            .flags = c_flags,
        });
        exe.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-fonts-cli.c"),
            .flags = c_flags,
        });

        b.installArtifact(exe);
    }

    // ocws-color: needs cairo
    {
        const exe = b.addExecutable(.{
            .name = "ocws-color",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-color.c"),
            .flags = c_flags,
        });

        exe.root_module.linkSystemLibrary("cairo", .{});
        exe.root_module.linkSystemLibrary("m", .{});
        b.installArtifact(exe);
    }

    // ocws-style: theme CSS generator
    {
        const exe = b.addExecutable(.{
            .name = "ocws-style",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-style.c"),
            .flags = c_flags,
        });

        b.installArtifact(exe);
    }

    // ocws-ocr: needs tesseract + leptonica
    {
        const exe = b.addExecutable(.{
            .name = "ocws-ocr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-ocr.c"),
            .flags = c_flags,
        });

        exe.root_module.linkSystemLibrary("tesseract", .{});
        exe.root_module.linkSystemLibrary("lept", .{});
        b.installArtifact(exe);
    }

    // ocws-notify: needs glib + gio
    {
        const exe = b.addExecutable(.{
            .name = "ocws-notify",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-notify.c"),
            .flags = c_flags,
        });

        exe.root_module.linkSystemLibrary("glib-2.0", .{});
        exe.root_module.linkSystemLibrary("gio-2.0", .{});
        exe.root_module.linkSystemLibrary("gobject-2.0", .{});
        b.installArtifact(exe);
    }

    // ocws-wallpaper: needs cairo + wayland-client
    {
        const exe = b.addExecutable(.{
            .name = "ocws-wallpaper",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-wallpaper.c"),
            .flags = c_flags,
        });

        exe.root_module.linkSystemLibrary("cairo", .{});
        b.installArtifact(exe);
    }

    // ocws-live-bg: GTK Layer Shell Live Background
    {
        const live_bg = b.addExecutable(.{
            .name = "ocws-live-bg",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        live_bg.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-live-bg.c"),
            .flags = c_flags,
        });
        live_bg.root_module.linkSystemLibrary("gtk+-3.0", .{});
        live_bg.root_module.linkSystemLibrary("gtk-layer-shell-0", .{});
        live_bg.root_module.linkSystemLibrary("m", .{});

        b.installArtifact(live_bg);
    }

    // ocws-dock-mgr: GTK3 Dock Manager GUI
    {
        const dock_mgr = b.addExecutable(.{
            .name = "ocws-dock-mgr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        dock_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-dock-mgr.c"),
            .flags = c_flags,
        });
        dock_mgr.root_module.linkSystemLibrary("gtk+-3.0", .{});
        dock_mgr.root_module.linkSystemLibrary("json-c", .{});

        b.installArtifact(dock_mgr);
    }

    // ocws-dotdesktop-mgr: GTK3 Desktop Entry Manager GUI
    {
        const dotdesktop_mgr = b.addExecutable(.{
            .name = "ocws-dotdesktop-mgr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        dotdesktop_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-dotdesktop-mgr.c"),
            .flags = c_flags,
        });
        dotdesktop_mgr.root_module.linkSystemLibrary("gtk+-3.0", .{});
        dotdesktop_mgr.root_module.linkSystemLibrary("glib-2.0", .{});
        dotdesktop_mgr.root_module.linkSystemLibrary("gio-2.0", .{});

        b.installArtifact(dotdesktop_mgr);
    }

    // ocws-osd-notify: GTK Layer Shell Notification Daemon
    {
        const osd_notify = b.addExecutable(.{
            .name = "ocws-osd-notify",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        osd_notify.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-osd-notify.c"),
            .flags = c_flags,
        });
        osd_notify.root_module.linkSystemLibrary("gtk+-3.0", .{});
        osd_notify.root_module.linkSystemLibrary("gtk-layer-shell-0", .{});
        osd_notify.root_module.linkSystemLibrary("gio-2.0", .{});
        osd_notify.root_module.linkSystemLibrary("glib-2.0", .{});

        b.installArtifact(osd_notify);
    }

    // ocws-hypertile: Dynamic Window Tiling Daemon
    {
        const hypertile = b.addExecutable(.{
            .name = "ocws-hypertile",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        hypertile.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-hypertile.c"),
            .flags = c_flags,
        });
        hypertile.root_module.linkSystemLibrary("wayland-client", .{});

        b.installArtifact(hypertile);
    }

    // ocws-brokerd: C-native Event Bus Daemon
    {
        const brokerd = b.addExecutable(.{
            .name = "ocws-brokerd",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        brokerd.root_module.addCSourceFile(.{
            .file = b.path("src/daemons/ocws-brokerd.c"),
            .flags = c_flags,
        });

        b.installArtifact(brokerd);
    }

    // ocws-welcome: GTK3 Welcome GUI
    {
        const welcome = b.addExecutable(.{
            .name = "ocws-welcome",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        welcome.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-welcome.c"),
            .flags = c_flags,
        });
        welcome.root_module.addCSourceFile(.{
            .file = b.path("src/core/utils.c"),
            .flags = c_flags,
        });
        welcome.root_module.linkSystemLibrary("gtk+-3.0", .{});
        welcome.root_module.linkSystemLibrary("glib-2.0", .{});

        b.installArtifact(welcome);
    }

    // ocws-workspace-mgr: GTK3 Workspace Manager
    {
        const wsmgr = b.addExecutable(.{
            .name = "ocws-workspace-mgr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        wsmgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-workspace-mgr.c"),
            .flags = c_flags,
        });
        wsmgr.root_module.addCSourceFile(.{
            .file = b.path("protocols/wlr-foreign-toplevel-management-unstable-v1-client.c"),
            .flags = c_flags,
        });
        wsmgr.root_module.linkSystemLibrary("gtk+-3.0", .{});
        wsmgr.root_module.linkSystemLibrary("glib-2.0", .{});
        wsmgr.root_module.linkSystemLibrary("wayland-client", .{});

        b.installArtifact(wsmgr);
    }

    // ocws-settings: GTK3 Settings GUI
    {
        const settings = b.addExecutable(.{
            .name = "ocws-settings",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        settings.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-settings.c"),
            .flags = c_flags,
        });
        settings.root_module.addCSourceFile(.{
            .file = b.path("src/gui/settings/settings-ui.c"),
            .flags = c_flags,
        });
        settings.root_module.addCSourceFile(.{
            .file = b.path("src/gui/settings/settings-tabs.c"),
            .flags = c_flags,
        });
        settings.root_module.addCSourceFile(.{
            .file = b.path("src/core/utils.c"),
            .flags = c_flags,
        });
        settings.root_module.linkSystemLibrary("gtk+-3.0", .{});
        settings.root_module.linkSystemLibrary("glib-2.0", .{});

        b.installArtifact(settings);
    }

    // ocws-fonts-mgr: GTK3 Fonts Manager GUI (modular)
    {
        const fonts_mgr = b.addExecutable(.{
            .name = "ocws-fonts-mgr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr-common.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr-fonts.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr-preview.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr-installer.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-fonts-mgr/fonts-mgr-ui.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.addCSourceFile(.{
            .file = b.path("src/core/ocws-fonts.c"),
            .flags = c_flags,
        });
        fonts_mgr.root_module.linkSystemLibrary("gtk+-3.0", .{});
        fonts_mgr.root_module.linkSystemLibrary("glib-2.0", .{});
        fonts_mgr.root_module.linkSystemLibrary("gio-2.0", .{});

        b.installArtifact(fonts_mgr);
    }

    // ocws-pkgmgr: GTK3 Package Manager GUI
    {
        const pkgmgr = b.addExecutable(.{
            .name = "ocws-pkgmgr",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        pkgmgr.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-pkgmgr.c"),
            .flags = c_flags,
        });
        pkgmgr.root_module.addCSourceFile(.{
            .file = b.path("src/core/utils.c"),
            .flags = c_flags,
        });
        pkgmgr.root_module.linkSystemLibrary("gtk+-3.0", .{});
        pkgmgr.root_module.linkSystemLibrary("glib-2.0", .{});

        b.installArtifact(pkgmgr);
    }

    // ocws-llm-runner: GTK3 LLM UI
    {
        const llm_runner = b.addExecutable(.{
            .name = "ocws-llm-runner",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        llm_runner.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-llm-runner.c"),
            .flags = c_flags,
        });
        llm_runner.root_module.linkSystemLibrary("gtk+-3.0", .{});
        llm_runner.root_module.linkSystemLibrary("json-c", .{});

        b.installArtifact(llm_runner);
    }

    // ocws-theme-center: GTK3 Theme Center GUI
    {
        const theme_center = b.addExecutable(.{
            .name = "ocws-theme-center",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });

        theme_center.root_module.addCSourceFile(.{
            .file = b.path("src/gui/ocws-theme-center.c"),
            .flags = c_flags,
        });
        theme_center.root_module.linkSystemLibrary("gtk+-3.0", .{});
        theme_center.root_module.linkSystemLibrary("glib-2.0", .{});

        b.installArtifact(theme_center);

        // ocws-plugin: Native plugin loader
        const plugin = b.addExecutable(.{
            .name = "ocws-plugin",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });
        plugin.root_module.addCSourceFile(.{
            .file = b.path("src/cli/ocws-plugin.c"),
            .flags = c_flags,
        });
        b.installArtifact(plugin);
        // ocws-appletd: Unified Applet Daemon
        const appletd = b.addExecutable(.{
            .name = "ocws-appletd",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });
        appletd.root_module.addCSourceFile(.{
            .file = b.path("src/daemon/ocws-appletd.c"),
            .flags = c_flags,
        });
        b.installArtifact(appletd);

        // --- libocws-store: reactive state library + tests ---
        const test_store = b.addExecutable(.{
            .name = "test_store",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
                .link_libc = true,
            }),
        });
        test_store.root_module.addIncludePath(b.path("src"));
        test_store.root_module.addCSourceFile(.{
            .file = b.path("src/libocws/store.c"),
            .flags = c_flags,
        });
        test_store.root_module.addCSourceFile(.{
            .file = b.path("src/tests/test_store.c"),
            .flags = c_flags,
        });
        test_store.root_module.linkSystemLibrary("glib-2.0", .{});
        test_store.root_module.linkSystemLibrary("gobject-2.0", .{});
        test_store.root_module.linkSystemLibrary("gio-2.0", .{});
        test_store.root_module.linkSystemLibrary("gtk+-3.0", .{});
        b.installArtifact(test_store);

        const run_store_test = b.addRunArtifact(test_store);
        const store_test_step = b.step("test-store", "Run libocws-store test suite");
        store_test_step.dependOn(&run_store_test.step);
    }
}
