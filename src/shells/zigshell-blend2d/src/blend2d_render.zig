// blend2d_render.zig — Blend2D rendering abstraction for Wayland SHM buffers
// Renders directly to an external pixel buffer (the mmap'd SHM buffer).

const std = @import("std");
const c = @import("c.zig").c;

pub const TextMetrics = struct {
    width: f64 = 0,
    height: f64 = 0,
};

pub const BlendRenderer = struct {
    image: c.BLImageCore,
    ctx: c.BLContextCore,
    font_face: c.BLFontFaceCore,
    font: c.BLFontCore,
    buf_width: i32 = 0,
    buf_height: i32 = 0,
    initialized: bool = false,

    pub fn init(pixel_data: [*]u8, width: i32, height: i32, stride_bytes: i32) !BlendRenderer {
        var self = BlendRenderer{
            .image = std.mem.zeroes(c.BLImageCore),
            .ctx = std.mem.zeroes(c.BLContextCore),
            .font_face = std.mem.zeroes(c.BLFontFaceCore),
            .font = std.mem.zeroes(c.BLFontCore),
        };
        self.buf_width = width;
        self.buf_height = height;

        // Create BLImage backed by external pixel data (zero-copy)
        // BL_FORMAT_PRGB32 = 1
        const result = c.bl_image_init_as_from_data(
            &self.image,
            @intCast(width),
            @intCast(height),
            @as(c_uint, c.BL_FORMAT_PRGB32),
            @ptrCast(pixel_data),
            @intCast(stride_bytes),
            @as(c_uint, 0x01), // BL_DATA_ACCESS_WRITE
            null,
            null,
        );
        if (result != @as(c_uint, c.BL_SUCCESS)) return error.Blend2DError;

        // Create rendering context
        const ctx_result = c.bl_context_init_as(&self.ctx, &self.image, null);
        if (ctx_result != @as(c_uint, c.BL_SUCCESS)) return error.Blend2DError;

        // Load default font (best-effort)
        self.loadDefaultFont() catch {};

        self.initialized = true;
        return self;
    }

    pub fn deinit(self: *BlendRenderer) void {
        if (!self.initialized) return;
        _ = c.bl_context_end(&self.ctx);
        _ = c.bl_context_destroy(&self.ctx);
        _ = c.bl_font_destroy(&self.font);
        _ = c.bl_font_face_destroy(&self.font_face);
        _ = c.bl_image_destroy(&self.image);
        self.initialized = false;
    }

    fn loadDefaultFont(self: *BlendRenderer) !void {
        const font_paths = [_][]const u8{
            // Debian/Ubuntu
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            // Fedora/RHEL
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
            // Arch
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
            // OpenMandriva
            "/usr/share/fonts/gnu-free/FreeSans.ttf",
            // Generic fallbacks
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        };

        for (font_paths) |path| {
            var path_buf: [256:0]u8 = undefined;
            const path_z = std.fmt.bufPrintZ(&path_buf, "{s}", .{path}) catch continue;

            const face_result = c.bl_font_face_create_from_file(&self.font_face, path_z.ptr, @as(c_int, 0));
            if (face_result == @as(c_uint, c.BL_SUCCESS)) {
                const font_result = c.bl_font_create_from_face(&self.font, &self.font_face, 11.0);
                if (font_result == @as(c_uint, c.BL_SUCCESS)) return;
            }
        }

        _ = c.bl_font_init(&self.font);
    }

    pub fn loadBoldFont(self: *BlendRenderer) void {
        const bold_paths = [_][]const u8{
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        };

        var bold_face = std.mem.zeroes(c.BLFontFaceCore);
        for (bold_paths) |path| {
            var path_buf: [256:0]u8 = undefined;
            const path_z = std.fmt.bufPrintZ(&path_buf, "{s}", .{path}) catch continue;
            if (c.bl_font_face_create_from_file(&bold_face, path_z.ptr, @as(c_int, 0)) == @as(c_uint, c.BL_SUCCESS)) {
                _ = c.bl_font_destroy(&self.font);
                _ = c.bl_font_create_from_face(&self.font, &bold_face, self.font_size());
                _ = c.bl_font_face_destroy(&bold_face);
                return;
            }
        }
        _ = c.bl_font_face_destroy(&bold_face);
    }

    pub fn loadRegularFont(self: *BlendRenderer) void {
        _ = self;
        // Reload the default font (regular weight)
        // This is a simplified version — in production you'd cache the regular face
    }

    pub fn font_size(self: *BlendRenderer) f64 {
        return c.bl_font_get_size(&self.font);
    }

    pub fn setFontSize(self: *BlendRenderer, size: f64) void {
        _ = c.bl_font_set_size(&self.font, @floatCast(size));
    }

    pub fn fillRect(self: *BlendRenderer, x: f64, y: f64, w: f64, h: f64, color: u32) void {
        const rect = c.BLRect{ .x = x, .y = y, .w = w, .h = h };
        _ = c.bl_context_set_fill_style_rgba32(&self.ctx, color);
        _ = c.bl_context_fill_rect_d(&self.ctx, &rect);
    }

    pub fn fillRectRaw(self: *BlendRenderer, x: f64, y: f64, w: f64, h: f64, r: u8, g: u8, b: u8, a: u8) void {
        const color: u32 = @as(u32, a) << 24 | @as(u32, r) << 16 | @as(u32, g) << 8 | @as(u32, b);
        self.fillRect(x, y, w, h, color);
    }

    pub fn drawText(self: *BlendRenderer, text: []const u8, x: f64, y: f64, color: u32) void {
        if (text.len == 0) return;

        var gb = std.mem.zeroes(c.BLGlyphBufferCore);
        _ = c.bl_glyph_buffer_init(&gb);
        defer _ = c.bl_glyph_buffer_destroy(&gb);

        // Set text (UTF8 = 0)
        _ = c.bl_glyph_buffer_set_text(&gb, text.ptr, text.len, @as(c_uint, 0));

        // Shape
        _ = c.bl_font_shape(&self.font, &gb);

        // Get glyph run
        const glyph_run = c.bl_glyph_buffer_get_glyph_run(&gb);

        // Draw glyphs
        const origin = c.BLPoint{ .x = x, .y = y };
        _ = c.bl_context_set_fill_style_rgba32(&self.ctx, color);
        _ = c.bl_context_fill_glyph_run_d(&self.ctx, &origin, &self.font, glyph_run);
    }

    pub fn measureText(self: *BlendRenderer, text: []const u8) TextMetrics {
        if (text.len == 0) return .{};

        var gb = std.mem.zeroes(c.BLGlyphBufferCore);
        _ = c.bl_glyph_buffer_init(&gb);
        defer _ = c.bl_glyph_buffer_destroy(&gb);

        _ = c.bl_glyph_buffer_set_text(&gb, text.ptr, text.len, @as(c_uint, 0));
        _ = c.bl_font_shape(&self.font, &gb);

        var tm = std.mem.zeroes(c.BLTextMetrics);
        _ = c.bl_font_get_text_metrics(&self.font, &gb, &tm);

        return .{
            .width = tm.bounding_box.x1 - tm.bounding_box.x0,
            .height = tm.bounding_box.y1 - tm.bounding_box.y0,
        };
    }

    pub fn drawImage(self: *BlendRenderer, img: *c.BLImageCore, x: f64, y: f64) void {
        const origin = c.BLPoint{ .x = x, .y = y };
        _ = c.bl_context_blit_image_d(&self.ctx, &origin, img, null);
    }

    pub fn drawCircle(self: *BlendRenderer, cx: f64, cy: f64, radius: f64, color: u32) void {
        var path = std.mem.zeroes(c.BLPathCore);
        _ = c.bl_path_init(&path);
        defer _ = c.bl_path_destroy(&path);

        // Approximate circle with bezier curves (4 arcs)
        const kappa = 0.5522847498;
        const kx = cx + radius * kappa;
        const ky = cy + radius * kappa;

        _ = c.bl_path_move_to(&path, cx + radius, cy);
        _ = c.bl_path_cubic_to(&path, kx, cy + radius, cx + radius, ky, cx + radius, cy + radius);
        _ = c.bl_path_cubic_to(&path, cx, cy + radius * (1 + kappa), cx - radius * (1 + kappa), cy, cx - radius, cy);
        _ = c.bl_path_cubic_to(&path, cx - radius * (1 + kappa), cy - radius, cx, cy - radius * (1 - kappa), cx, cy - radius);
        _ = c.bl_path_cubic_to(&path, cx, cy - radius * (1 + kappa), cx + radius * (1 + kappa), cy, cx + radius, cy);
        _ = c.bl_path_close(&path);

        _ = c.bl_context_set_fill_style_rgba32(&self.ctx, color);
        _ = c.bl_context_fill_path_d(&self.ctx, &c.BLPoint{ .x = 0, .y = 0 }, &path);
    }

    pub fn drawBorder(self: *BlendRenderer, x: f64, y: f64, w: f64, h: f64, color: u32) void {
        _ = c.bl_context_set_stroke_style_rgba32(&self.ctx, color);
        _ = c.bl_context_set_stroke_width(&self.ctx, 1.0);
        const rect = c.BLRect{ .x = x, .y = y, .w = w, .h = h };
        _ = c.bl_context_stroke_rect_d(&self.ctx, &rect);
    }
};
