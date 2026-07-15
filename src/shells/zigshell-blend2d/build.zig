const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const c_flags: []const []const u8 = &.{ "-std=gnu11", "-Wall" };

    // Build options
    const static = b.option(bool, "static", "Build Blend2D as static library") orelse false;

    // Step 1: Build Blend2D via CMake
    var cmake_base_args = [_][]const u8{ "cmake", "-B", "build/deps", "-S", ".", "-DCMAKE_BUILD_TYPE=Release", "-DBLEND2D_NO_JIT=ON" };
    var cmake_all_args: [8][]const u8 = undefined;
    var cmake_arg_count: usize = cmake_base_args.len;

    @memcpy(cmake_all_args[0..cmake_base_args.len], &cmake_base_args);
    if (static) {
        cmake_all_args[cmake_arg_count] = "-DBLEND2D_TARGET_TYPE=STATIC";
        cmake_arg_count += 1;
    }

    const cmake_configure = b.addSystemCommand(cmake_all_args[0..cmake_arg_count]);
    const cmake_build = b.addSystemCommand(&.{ "make", "-C", "build/deps", "blend2d", "-j4" });
    cmake_build.step.dependOn(&cmake_configure.step);

    // Step 2: Build the Zig shell executable
    const root_mod = b.createModule(.{
        .root_source_file = b.path("src/main_shell.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    const exe = b.addExecutable(.{
        .name = "zigshell-blend2d",
        .root_module = root_mod,
    });
    exe.step.dependOn(&cmake_build.step);

    // Link Wayland
    root_mod.linkSystemLibrary("wayland-client", .{});
    root_mod.addIncludePath(b.path("src"));
    root_mod.addIncludePath(b.path("."));
    root_mod.addIncludePath(b.path("deps/blend2d"));

    // Link Blend2D
    root_mod.addLibraryPath(b.path("build/deps/blend2d"));
    if (static) {
        root_mod.linkSystemLibrary("blend2d", .{});
    } else {
        root_mod.linkSystemLibrary("blend2d", .{});
    }
    root_mod.linkSystemLibrary("stdc++", .{});

    // Add protocol C sources
    addProtocolSources(root_mod, b, c_flags);

    // Add dock_c_impl.c (includes Blend2D implementation)
    root_mod.addCSourceFile(.{
        .file = b.path("src/dock_c_impl.c"),
        .flags = &.{ "-std=gnu11", "-Wall", "-DBLEND2D_STATIC" },
    });

    b.installArtifact(exe);

    // Run step
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);

    const run_step = b.step("run", "Run zigshell-blend2d");
    run_step.dependOn(&run_cmd.step);

    // Test step — build and run a basic smoke test
    const test_mod = b.createModule(.{
        .root_source_file = b.path("src/main_shell.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const test_exe = b.addTest(.{
        .root_module = test_mod,
    });
    const run_test = b.addRunArtifact(test_exe);
    run_test.step.dependOn(&cmake_build.step);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_test.step);
}

fn addProtocolSources(root_mod: *std.Build.Module, b: *std.Build, c_flags: []const []const u8) void {
    root_mod.addCSourceFile(.{
        .file = b.path("wlr-layer-shell-unstable-v1-client-protocol.c"),
        .flags = c_flags,
    });
    root_mod.addCSourceFile(.{
        .file = b.path("wlr-foreign-toplevel-management-unstable-v1-client-protocol.c"),
        .flags = c_flags,
    });
    root_mod.addCSourceFile(.{
        .file = b.path("xdg-shell-client-protocol.c"),
        .flags = c_flags,
    });
}
