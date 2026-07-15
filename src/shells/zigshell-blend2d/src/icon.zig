// icon.zig — Desktop file lookup + PNG icon loading via Blend2D
// Replaces librsvg with Blend2D's built-in image codec.
// SVG support is dropped (Blend2D doesn't load SVG).
// Fallback: colored circle with first letter.

const std = @import("std");
const c = @import("c.zig").c;
const blend2d = @import("blend2d_render.zig");

const desktop_dirs = [_][]const u8{
    "/usr/share/applications/",
    "/usr/local/share/applications/",
};

const theme_dirs = [_][]const u8{
    "/usr/share/icons/hicolor/{d}x{d}/apps/",
    "/usr/local/share/icons/hicolor/{d}x{d}/apps/",
    "/usr/share/icons/Papirus/{d}x{d}/apps/",
    "/usr/share/icons/Papirus-Dark/{d}x{d}/apps/",
    "/usr/share/icons/breeze/apps/{d}/",
    "/usr/share/icons/breeze-dark/apps/{d}/",
    "/usr/share/icons/gnome/{d}x{d}/apps/",
    "/usr/share/icons/Adwaita/{d}x{d}/apps/",
};

const scalable_dirs = [_][]const u8{
    "/usr/share/icons/hicolor/scalable/apps/",
    "/usr/local/share/icons/hicolor/scalable/apps/",
    "/usr/share/icons/Papirus/scalable/apps/",
    "/usr/share/icons/Papirus-Dark/scalable/apps/",
    "/usr/share/icons/breeze/apps/scalable/",
    "/usr/share/icons/breeze-dark/apps/scalable/",
    "/usr/share/icons/gnome/scalable/apps/",
    "/usr/share/icons/Adwaita/scalable/apps/",
};

const sizes = [_]i32{ 48, 32, 24, 22, 16, 64, 96, 128, 256 };

// Icon cache
const CacheEntry = struct {
    app_id: [128]u8,
    img: ?c.BLImageCore,
};

const ICON_CACHE_MAX = 64;
var icon_cache: [ICON_CACHE_MAX]CacheEntry = std.mem.zeroes([ICON_CACHE_MAX]CacheEntry);
var icon_cache_count: i32 = 0;

pub fn clearCache() void {
    for (0..@intCast(icon_cache_count)) |i| {
        if (icon_cache[i].img) |*img| {
            _ = c.bl_image_destroy(img);
        }
    }
    icon_cache_count = 0;
}

fn pathExists(path: [*:0]const u8) bool {
    const f = c.fopen(path, "r") orelse return false;
    _ = c.fclose(f);
    return true;
}

fn findDesktopFile(app_id: [*:0]const u8) ?[512:0]u8 {
    var buf: [512:0]u8 = std.mem.zeroes([512:0]u8);
    const id_slice = std.mem.sliceTo(app_id, 0);

    for (desktop_dirs) |dir| {
        const path = std.fmt.bufPrintZ(&buf, "{s}{s}.desktop", .{ dir, id_slice }) catch continue;
        if (pathExists(path.ptr)) return buf;

        var alt: [128]u8 = std.mem.zeroes([128]u8);
        var alt_len: usize = 0;
        for (id_slice) |ch| {
            if (alt_len < alt.len) {
                alt[alt_len] = if (ch == '-') '.' else ch;
                alt_len += 1;
            }
        }
        if (alt_len > 0) {
            const alt_path = std.fmt.bufPrintZ(&buf, "{s}{s}.desktop", .{ dir, alt[0..alt_len] }) catch continue;
            if (pathExists(alt_path.ptr)) return buf;
        }
    }
    return null;
}

fn readIconName(desktop_path: [*:0]const u8) ?[128:0]u8 {
    var icon_name: [128:0]u8 = std.mem.zeroes([128:0]u8);
    var generic_name: [128:0]u8 = std.mem.zeroes([128:0]u8);
    const f = c.fopen(desktop_path, "r") orelse return null;
    defer _ = c.fclose(f);

    var line: [512]u8 = std.mem.zeroes([512]u8);
    var in_entry = false;

    while (c.fgets(@ptrCast(&line), @intCast(line.len), f) != null) {
        const line_len = std.mem.indexOfScalar(u8, &line, 0) orelse line.len;
        const real_line = line[0..line_len];

        if (real_line.len > 0 and real_line[0] == '[') {
            if (in_entry) break;
            in_entry = true;
            continue;
        }
        if (std.mem.startsWith(u8, real_line, "Icon=")) {
            var val = real_line[5..];
            while (val.len > 0 and (val[val.len - 1] == '\n' or val[val.len - 1] == '\r')) {
                val = val[0 .. val.len - 1];
            }
            const len = @min(val.len, icon_name.len - 1);
            @memcpy(icon_name[0..len], val[0..len]);
            icon_name[len] = 0;
        } else if (std.mem.startsWith(u8, real_line, "GenericName=")) {
            var val = real_line[12..];
            while (val.len > 0 and (val[val.len - 1] == '\n' or val[val.len - 1] == '\r')) {
                val = val[0 .. val.len - 1];
            }
            const len = @min(val.len, generic_name.len - 1);
            @memcpy(generic_name[0..len], val[0..len]);
            generic_name[len] = 0;
        }
    }
    // Prefer Icon=, fallback to GenericName=
    if (icon_name[0] != 0) return icon_name;
    if (generic_name[0] != 0) return generic_name;
    return null;
}

fn buildPath(buf: *[1024:0]u8, dir: []const u8, name: []const u8, ext: []const u8) bool {
    var pos: usize = 0;
    for (dir) |c_val| {
        if (pos < buf.len) {
            buf[pos] = c_val;
            pos += 1;
        }
    }
    for (name) |c_val| {
        if (pos < buf.len) {
            buf[pos] = c_val;
            pos += 1;
        }
    }
    for (ext) |c_val| {
        if (pos < buf.len) {
            buf[pos] = c_val;
            pos += 1;
        }
    }
    if (pos < buf.len) {
        buf[pos] = 0;
        return true;
    }
    return false;
}

fn buildSizedPath(buf: *[1024:0]u8, dir_fmt: []const u8, size1: i32, size2: i32, name: []const u8, ext: []const u8) bool {
    var pos: usize = 0;
    var size_idx: u8 = 0;
    var i: usize = 0;
    while (i < dir_fmt.len) : (i += 1) {
        if (dir_fmt[i] == '%' and i + 1 < dir_fmt.len and dir_fmt[i + 1] == 'd') {
            const val: usize = @intCast(if (size_idx == 0) size1 else size2);
            size_idx += 1;
            var digit_buf: [16]u8 = undefined;
            const digit_str = std.fmt.bufPrint(&digit_buf, "{d}", .{val}) catch return false;
            for (digit_str) |d| {
                if (pos < buf.len) {
                    buf[pos] = d;
                    pos += 1;
                }
            }
            i += 1;
        } else {
            if (pos < buf.len) {
                buf[pos] = dir_fmt[i];
                pos += 1;
            }
        }
    }
    for (name) |c_val| {
        if (pos < buf.len) {
            buf[pos] = c_val;
            pos += 1;
        }
    }
    for (ext) |c_val| {
        if (pos < buf.len) {
            buf[pos] = c_val;
            pos += 1;
        }
    }
    if (pos < buf.len) {
        buf[pos] = 0;
        return true;
    }
    return false;
}

fn tryLoadPng(icon_name: [*:0]const u8) ?c.BLImageCore {
    // Try scalable directories first
    for (scalable_dirs) |dir| {
        var buf: [1024:0]u8 = std.mem.zeroes([1024:0]u8);
        if (!buildPath(&buf, dir, std.mem.sliceTo(icon_name, 0), ".png")) continue;
        const buf_ptr: [*:0]const u8 = @ptrCast(&buf);
        var img = std.mem.zeroes(c.BLImageCore);
        if (c.bl_image_read_from_file(&img, buf_ptr, null) == @as(c_uint, c.BL_SUCCESS)) return img;
        _ = c.bl_image_destroy(&img);
    }

    // Try sized directories
    for (sizes) |s| {
        for (theme_dirs) |dir| {
            var buf: [1024:0]u8 = std.mem.zeroes([1024:0]u8);
            if (!buildSizedPath(&buf, dir, s, s, std.mem.sliceTo(icon_name, 0), ".png")) continue;
            const buf_ptr: [*:0]const u8 = @ptrCast(&buf);
            var img = std.mem.zeroes(c.BLImageCore);
            if (c.bl_image_read_from_file(&img, buf_ptr, null) == @as(c_uint, c.BL_SUCCESS)) return img;
            _ = c.bl_image_destroy(&img);
        }
    }
    return null;
}

pub fn load(app_id: [*:0]const u8, size: i32) ?c.BLImageCore {
    _ = size; // Blend2D handles scaling at draw time
    const app_id_slice = std.mem.sliceTo(app_id, 0);

    // Check cache first
    for (0..@intCast(icon_cache_count)) |i| {
        if (std.mem.eql(u8, &icon_cache[i].app_id, app_id_slice)) {
            return icon_cache[i].img;
        }
    }

    var icon_name_ptr: [*:0]const u8 = app_id;
    var desktop_buf: ?[512:0]u8 = findDesktopFile(app_id);
    if (desktop_buf) |*db| {
        if (readIconName(db.ptr)) |name| {
            icon_name_ptr = @ptrCast(&name);
        }
    }

    var img = tryLoadPng(icon_name_ptr);
    if (img) |loaded| {
        cacheIcon(app_id_slice, loaded);
        return loaded;
    }

    // Try app_id directly
    if (icon_name_ptr != app_id) {
        img = tryLoadPng(app_id);
        if (img) |loaded| {
            cacheIcon(app_id_slice, loaded);
            return loaded;
        }
    }

    return null;
}

fn cacheIcon(app_id: []const u8, img: c.BLImageCore) void {
    if (icon_cache_count >= ICON_CACHE_MAX) return;
    const idx: usize = @intCast(icon_cache_count);
    icon_cache_count += 1;
    const len = @min(app_id.len, 127);
    @memcpy(icon_cache[idx].app_id[0..len], app_id[0..len]);
    icon_cache[idx].app_id[len] = 0;
    icon_cache[idx].img = img;
}

fn hueForString(s: []const u8) f64 {
    var h: u32 = 0;
    for (s) |c_val| {
        h = h *| 31 +| @as(u32, c_val);
    }
    return @as(f64, @floatFromInt(@mod(h, 360))) / 360.0;
}

pub fn fallback(app_id: [*:0]const u8, size: i32) c.BLImageCore {
    // Create a colored circle with first letter as fallback icon
    var img = std.mem.zeroes(c.BLImageCore);
    _ = c.bl_image_init_as(&img, @intCast(size), @intCast(size), @as(c_uint, c.BL_FORMAT_PRGB32));

    var ctx = std.mem.zeroes(c.BLContextCore);
    _ = c.bl_context_init_as(&ctx, &img, null);

    const app_id_slice = std.mem.sliceTo(app_id, 0);
    const hue = hueForString(app_id_slice);

    // HSV to RGB
    const h6 = hue * 6;
    const sext: i32 = @intFromFloat(h6);
    const frac = h6 - @as(f64, @floatFromInt(sext));
    const v: f64 = 0.6;
    const s: f64 = 0.7;
    const p = v * (1 - s);
    const q = v * (1 - s * frac);
    const t = v * (1 - s * (1 - frac));

    var r: f64 = undefined;
    var g: f64 = undefined;
    var b: f64 = undefined;
    switch (@mod(sext, 6)) {
        0 => { r = v; g = t; b = p; },
        1 => { r = q; g = v; b = p; },
        2 => { r = p; g = v; b = t; },
        3 => { r = p; g = q; b = v; },
        4 => { r = t; g = p; b = v; },
        else => { r = v; g = p; b = q; },
    }

    const color: u32 = @as(u32, 255) << 24 | @as(u32, @intFromFloat(r * 255)) << 16 | @as(u32, @intFromFloat(g * 255)) << 8 | @as(u32, @intFromFloat(b * 255));

    // Draw circle using bezier path
    const cx = @as(f64, @floatFromInt(size)) / 2.0;
    const cy = @as(f64, @floatFromInt(size)) / 2.0;
    const radius = @as(f64, @floatFromInt(size)) / 2.0 - 2.0;
    const kappa = 0.5522847498;

    var path = std.mem.zeroes(c.BLPathCore);
    _ = c.bl_path_init(&path);

    _ = c.bl_path_move_to(&path, cx + radius, cy);
    _ = c.bl_path_cubic_to(&path, cx + radius, cy + radius * kappa, cx + radius * kappa, cy + radius, cx, cy + radius);
    _ = c.bl_path_cubic_to(&path, cx - radius * kappa, cy + radius, cx - radius, cy + radius * kappa, cx - radius, cy);
    _ = c.bl_path_cubic_to(&path, cx - radius, cy - radius * kappa, cx - radius * kappa, cy - radius, cx, cy - radius);
    _ = c.bl_path_cubic_to(&path, cx + radius * kappa, cy - radius, cx + radius, cy - radius * kappa, cx + radius, cy);
    _ = c.bl_path_close(&path);

    _ = c.bl_context_set_fill_style_rgba32(&ctx, color);
    _ = c.bl_context_fill_path_d(&ctx, &c.BLPoint{ .x = 0, .y = 0 }, &path);
    _ = c.bl_path_destroy(&path);

    // Draw first letter (if we have a font loaded)
    if (app_id_slice.len > 0) {
        var letter: [8]u8 = undefined;
        const first_char = std.ascii.toUpper(app_id_slice[0]);
        letter[0] = first_char;
        letter[1] = 0;

        var gb = std.mem.zeroes(c.BLGlyphBufferCore);
        _ = c.bl_glyph_buffer_init(&gb);
        _ = c.bl_glyph_buffer_set_text(&gb, &letter, 1, @as(c_uint, 0));

        // Try to load a font for the letter
        var font_face = std.mem.zeroes(c.BLFontFaceCore);
        var font = std.mem.zeroes(c.BLFontCore);
        const font_paths = [_][]const u8{
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        };

        for (font_paths) |fp| {
            var path_buf: [256:0]u8 = undefined;
            const path_z = std.fmt.bufPrintZ(&path_buf, "{s}", .{fp}) catch continue;
            if (c.bl_font_face_create_from_file(&font_face, path_z.ptr, @as(c_int, 0)) == @as(c_uint, c.BL_SUCCESS)) {
                const font_size = @as(f64, @floatFromInt(size)) * 0.55;
                _ = c.bl_font_create_from_face(&font, &font_face, @floatCast(font_size));
                _ = c.bl_font_shape(&font, &gb);

                const glyph_run = c.bl_glyph_buffer_get_glyph_run(&gb);
                _ = c.bl_context_set_fill_style_rgba32(&ctx, 0xFFFFFFFF); // white
                _ = c.bl_context_fill_glyph_run_d(&ctx, &c.BLPoint{ .x = cx - 4, .y = cy + 4 }, &font, glyph_run);

                _ = c.bl_font_destroy(&font);
                _ = c.bl_font_face_destroy(&font_face);
                break;
            }
        }

        _ = c.bl_font_face_destroy(&font_face);
        _ = c.bl_font_destroy(&font);
        _ = c.bl_glyph_buffer_destroy(&gb);
    }

    _ = c.bl_context_end(&ctx);
    _ = c.bl_context_destroy(&ctx);

    return img;
}
