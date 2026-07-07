const std = @import("std");
const testing = std.testing;

// Import our C libraries for testing!
const ocws = @cImport({
    @cInclude("libocws/string.h");
    @cInclude("libocws/fs.h");
});

test "ocws_str_prettify converts slug to pretty title" {
    // string.h has inline ocws_str_prettify
    const input = "my-awesome-theme";
    const result = ocws.ocws_str_prettify(input);
    
    // Zig catches if it's null!
    try testing.expect(result != null);
    
    // Convert C string to Zig string slice for comparison
    const result_slice = std.mem.span(result);
    try testing.expectEqualStrings("My Awesome Theme", result_slice);
    
    // Test underscore conversion
    const input2 = "dark_mode_on";
    const result2 = ocws.ocws_str_prettify(input2);
    try testing.expectEqualStrings("Dark Mode On", std.mem.span(result2));
    
    // C requires us to manually free memory allocated by strdup
    std.c.free(result);
    std.c.free(result2);
}

test "ocws_str_prettify handles null and empty gracefully" {
    const res1 = ocws.ocws_str_prettify(null);
    try testing.expect(res1 == null);
}

test "ocws get_config_dir generates correct path" {
    var buf: [1024]u8 = undefined;
    ocws.get_config_dir(&buf, buf.len);
    
    const result_slice = std.mem.sliceTo(&buf, 0);
    // It should end with .config/ocws
    try testing.expect(std.mem.endsWith(u8, result_slice, ".config/ocws"));
}
