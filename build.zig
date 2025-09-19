const std = @import("std");

const compile_commands = @import("build/compile_commands.zig");

pub fn build(b: *std.Build) !void {
    const comp_flags: []const []const u8 = &.{
        "-Werror",
        "-Wall",
        "-Weverything",
        "-Wpedantic",
        "-gen-cdb-fragment-path",
        b.fmt("{s}/{s}", .{ b.cache_root.path.?, "cdb" }),
    };
    const c_flags: []const []const u8 = try std.mem.concat(b.allocator, []const u8, &.{
        comp_flags,
        &.{
            "-std=c23",
            "-Wno-pre-c23-compat",
        },
    });
    _ = c_flags; // autofix
    const cpp_flags: []const []const u8 = try std.mem.concat(b.allocator, []const u8, &.{
        comp_flags,
        &.{
            "-std=c++23",
            "-Wno-c++98-compat",
            "-Wno-c++98-compat-pedantic",
            "-Wno-non-virtual-dtor",
            "-Wno-shadow-field-in-constructor",
            "-Wno-padded",
        },
    });

    const optimize = b.standardOptimizeOption(.{});
    // For builds meant to run on the host system
    const native_target = b.standardTargetOptions(.{});
    // For builds meant to run under the WINE prefix
    const wine_target = b.resolveTargetQuery(.{
        .os_tag = .windows,
        .os_version_min = .{ .windows = .win10 },
        .cpu_arch = .x86_64,
    });

    const os_openvr_name = switch (native_target.result.os.tag) {
        .windows => "win",
        .linux => "linux",
        else => @panic("unsupported os"),
    };
    const cpu_openvr_name = switch (native_target.result.cpu.arch) {
        .x86_64 => "64",
        else => @panic("unsupported arch"),
    };

    const openvr_dep = b.dependency("openvr", .{});
    const openvr_headers = openvr_dep.path("headers");

    const driver_ignition_path = b.path("projects/driver_ignition/");

    const external_openvr_path = b.path("externals/openvr/");

    const driver_ignition_module = b.createModule(.{
        .target = native_target,
        .optimize = optimize,
        .link_libcpp = true,
        .link_libc = true,
    });
    driver_ignition_module.addIncludePath(openvr_headers);
    driver_ignition_module.addIncludePath(external_openvr_path);
    driver_ignition_module.addCSourceFiles(.{
        .flags = cpp_flags,
        .files = &.{
            "driver.cpp",
            "server_tracked_devices_provider.cpp",
        },
        .language = .cpp,
        .root = driver_ignition_path,
    });

    const driver_ignition_dll = b.addLibrary(.{
        .name = "driver_ignition",
        .linkage = .dynamic,
        .root_module = driver_ignition_module,
    });

    const ignition_server_path = b.path("projects/win32/ignition_server/");

    const ignition_server_module = b.createModule(.{
        .target = wine_target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    ignition_server_module.addIncludePath(openvr_headers);
    ignition_server_module.addIncludePath(external_openvr_path);
    ignition_server_module.addCSourceFiles(.{
        .files = &.{
            "main.cpp",
            "properties.cpp",
            "resources.cpp",
            "settings.cpp",
            "driver_manager.cpp",
            "driver_log.cpp",
            "driver_input.cpp",
            "driver_host.cpp",
            "driver_context.cpp",
        },
        .root = ignition_server_path,
        .language = .cpp,
    });

    const ignition_server_exe = b.addExecutable(.{
        .name = "ignition_server",
        .root_module = ignition_server_module,
    });

    const install_driver_ignition = b.addInstallFile(driver_ignition_dll.getEmittedBin(), b.fmt("driver_ignition/bin/{s}{s}/{s}.{s}", .{
        os_openvr_name,
        cpu_openvr_name,
        driver_ignition_dll.name,
        switch (native_target.result.os.tag) {
            .windows => "dll",
            .linux => "so",
            else => @panic("unsupported os"),
        },
    }));

    b.installArtifact(ignition_server_exe);
    const install_step = b.getInstallStep();
    install_step.dependOn(&install_driver_ignition.step);

    // Compile commands
    const cc_step = b.step("cc", "Generate Compile Commands Database");
    const gen_file_step = try compile_commands.createStep(
        b,
        b.fmt("{s}/{s}", .{ b.cache_root.path orelse "./", "cdb" }),
        "compile_commands.json",
    );
    gen_file_step.dependOn(&driver_ignition_dll.step);
    cc_step.dependOn(gen_file_step);
}
