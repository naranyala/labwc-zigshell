const std = @import("std");
const testing = std.testing;

const ocws = @cImport({
    @cInclude("libocws/string.h");
    @cInclude("libocws/fs.h");
    @cInclude("libocws/easing.h");
});

// ============================================================
// string.h tests
// ============================================================

test "ocws_str_prettify converts slug to pretty title" {
    const input = "my-awesome-theme";
    const result = ocws.ocws_str_prettify(input);
    try testing.expect(result != null);
    const result_slice = std.mem.span(result);
    try testing.expectEqualStrings("My Awesome Theme", result_slice);
    std.c.free(result);
}

test "ocws_str_prettify handles underscores" {
    const result = ocws.ocws_str_prettify("dark_mode_on");
    try testing.expect(result != null);
    try testing.expectEqualStrings("Dark Mode On", std.mem.span(result));
    std.c.free(result);
}

test "ocws_str_prettify handles null and empty gracefully" {
    const res1 = ocws.ocws_str_prettify(null);
    try testing.expect(res1 == null);

    const res2 = ocws.ocws_str_prettify("");
    try testing.expect(res2 != null);
    try testing.expectEqualStrings("", std.mem.span(res2));
    std.c.free(res2);
}

test "ocws_str_prettify handles single word" {
    const result = ocws.ocws_str_prettify("catppuccin");
    try testing.expect(result != null);
    try testing.expectEqualStrings("Catppuccin", std.mem.span(result));
    std.c.free(result);
}

test "ocws_str_prettify handles mixed separators" {
    const result = ocws.ocws_str_prettify("my-cool_theme");
    try testing.expect(result != null);
    try testing.expectEqualStrings("My Cool Theme", std.mem.span(result));
    std.c.free(result);
}

// ============================================================
// fs.h tests
// ============================================================

test "ocws get_config_dir generates correct path" {
    var buf: [1024]u8 = undefined;
    ocws.get_config_dir(&buf, buf.len);
    const result_slice = std.mem.sliceTo(&buf, 0);
    try testing.expect(std.mem.endsWith(u8, result_slice, ".config/ocws"));
}

test "ocws get_config_dir respects buffer size" {
    var buf: [10]u8 = undefined;
    ocws.get_config_dir(&buf, buf.len);
    // Should not crash, buffer is truncated safely
}

// ============================================================
// easing.h tests
// ============================================================

test "ease_out_cubic at t=0 is 0" {
    const result = ocws.ease_out_cubic(0.0);
    try testing.expectApproxEqAbs(@as(f64, 0.0), result, 0.001);
}

test "ease_out_cubic at t=1 is 1" {
    const result = ocws.ease_out_cubic(1.0);
    try testing.expectApproxEqAbs(@as(f64, 1.0), result, 0.001);
}

test "ease_out_cubic at t=0.5 is ~0.875" {
    const result = ocws.ease_out_cubic(0.5);
    try testing.expectApproxEqAbs(@as(f64, 0.875), result, 0.001);
}

test "ease_in_out_cubic at t=0 is 0" {
    const result = ocws.ease_in_out_cubic(0.0);
    try testing.expectApproxEqAbs(@as(f64, 0.0), result, 0.001);
}

test "ease_in_out_cubic at t=1 is 1" {
    const result = ocws.ease_in_out_cubic(1.0);
    try testing.expectApproxEqAbs(@as(f64, 1.0), result, 0.001);
}

test "ease_in_out_cubic at t=0.5 is 0.5" {
    const result = ocws.ease_in_out_cubic(0.5);
    try testing.expectApproxEqAbs(@as(f64, 0.5), result, 0.001);
}

test "ease_in_out at t=0 is 0" {
    const result = ocws.ease_in_out(0.0);
    try testing.expectApproxEqAbs(@as(f64, 0.0), result, 0.001);
}

test "ease_in_out at t=1 is 1" {
    const result = ocws.ease_in_out(1.0);
    try testing.expectApproxEqAbs(@as(f64, 1.0), result, 0.001);
}
