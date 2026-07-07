#!/bin/bash
#
# theme-engine — Generate all config files from a theme INI profile
#
# Usage:
#   theme-engine apply <theme.ini> [--profile standard|full]   Apply theme and set profile
#   theme-engine preview <theme.ini>                           Show what would be generated
#   theme-engine list                        List available themes
#   theme-engine current                     Show active theme
#   theme-engine export <theme.ini>          Export generated files to dotfiles/
#
# Themes are INI files in themes/ with sections:
#   [meta], [colors], [labwc], [gtk3], [gtk4], [fonts],
#

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

info() { echo -e "  ${CYAN}→${NC} $1" >&2; }
pass()  { echo -e "  ${GREEN}✓${NC} $1" >&2; }
fail()  { echo -e "  ${RED}✗${NC} $1" >&2; exit 1; }
warn()  { echo -e "  ${YELLOW}⚠${NC} $*" >&2; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Find project root: check env var, then walk up from script dir
if [[ -n "${LABWC_PROJECT:-}" && -d "$LABWC_PROJECT/themes" ]]; then
    PROJECT_DIR="$LABWC_PROJECT"
else
    PROJECT_DIR="$SCRIPT_DIR"
    while [[ ! -d "$PROJECT_DIR/themes" && "$PROJECT_DIR" != "/" ]]; do
        PROJECT_DIR="$(dirname "$PROJECT_DIR")"
    done
fi

[[ -d "$PROJECT_DIR/themes" ]] || fail "Cannot find project root (themes/ not found)"
THEMES_DIR="$PROJECT_DIR/themes"
TEMPLATES_DIR="$PROJECT_DIR/templates"
DOTFILES_DIR="$PROJECT_DIR/dotfiles"

# ============================================================
# INI Parser — reads INI into associative arrays
# ============================================================

declare -A INI_VALUES

parse_ini() {
    local file="$1"
    local section=""

    [[ -f "$file" ]] || fail "Theme not found: $file"

    while IFS= read -r line || [[ -n "$line" ]]; do
        # Strip whitespace
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"

        # Skip empty/comment lines
        [[ -z "$line" || "$line" == \#* ]] && continue

        # Section header
        if [[ "$line" =~ ^\[([a-zA-Z0-9_]+)\]$ ]]; then
            section="${BASH_REMATCH[1]}"
            continue
        fi

        # Key=Value
        if [[ "$line" =~ ^([a-zA-Z0-9_]+)=(.+)$ ]]; then
            local key="${BASH_REMATCH[1]}"
            local val="${BASH_REMATCH[2]}"
            # Strip surrounding quotes
            val="${val#\"}"
            val="${val%\"}"
            val="${val#\'}"
            val="${val%\'}"
            INI_VALUES["${section}.${key}"]="$val"
        fi
    done < "$file"
}

# Get value with section.key, with optional default
ini_get() {
    local key="$1" default="${2:-}"
    echo "${INI_VALUES[$key]:-$default}"
}

# ============================================================
# Variable Expansion — resolves ${ref} in values
# ============================================================

expand_vars() {
    local max_depth=5
    for ((depth=1; depth<=max_depth; depth++)); do
        local changed
        changed=false
        for key in "${!INI_VALUES[@]}"; do
            local val="${INI_VALUES[$key]}"
            local new_val="$val"

            # Replace ${section.key} or ${key} references (defaulting to colors.key)
            local regex='\$\{([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?\}'
            while [[ "$new_val" =~ $regex ]]; do
                local ref_part1="${BASH_REMATCH[1]}"
                local ref_part2="${BASH_REMATCH[2]:-}"
                
                local ref_section
                local ref_key
                
                if [[ -z "$ref_part2" ]]; then
                    # No dot, assume colors section
                    ref_section="colors"
                    ref_key="$ref_part1"
                else
                    # Has dot, e.g. section.key
                    ref_section="$ref_part1"
                    ref_key="${ref_part2#.}"
                fi
                
                local ref_val="${INI_VALUES[${ref_section}.${ref_key}]:-}"

                if [[ -n "$ref_val" ]]; then
                    if [[ -z "$ref_part2" ]]; then
                        new_val="${new_val//\$\{${ref_key}\}/$ref_val}"
                    else
                        new_val="${new_val//\$\{${ref_section}.${ref_key}\}/$ref_val}"
                    fi
                    changed=true
                else
                    warn "Undefined reference: \${${ref_section}.${ref_key}} in $key"
                    break # prevent infinite loop on undefined reference
                fi
            done

            if [[ "$new_val" != "$val" ]]; then
                INI_VALUES["$key"]="$new_val"
            fi
        done

        if [[ "$changed" == false ]]; then
            break
        fi
    done
}

# ============================================================
# Template Rendering
# ============================================================

render_template() {
    local template_file="$1"
    local content=""

    [[ -f "$template_file" ]] || warn "Template not found: $template_file"
    content="$(<"$template_file")"

    # Replace {{VARIABLE}} references
    local tpl_regex='\{\{([A-Z_][A-Z0-9_]+)\}\}'
    while [[ "$content" =~ $tpl_regex ]]; do
        local var_name="${BASH_REMATCH[1]}"
        local var_value=""

                case "$var_name" in
                    THEME_NAME)      var_value=$(ini_get "meta.name" "$(basename "${theme_file:-$template_file}" .ini)" ) ;;
                    COLOR_BG)        var_value=$(ini_get "colors.bg" "#1e1e2e") ;;
                    COLOR_FG)        var_value=$(ini_get "colors.fg" "#cdd6f4") ;;
                    COLOR_SURFACE)   var_value=$(ini_get "colors.surface" "#1e1e2e") ;;
                    COLOR_BORDER)    var_value=$(ini_get "colors.border" "#45475a") ;;
                    COLOR_ACCENT)    var_value=$(ini_get "colors.accent" "#89b4fa") ;;
                    COLOR_URGENT)     var_value=$(ini_get "colors.urgent" "#f38ba8") ;;
                    COLOR_OK)         var_value=$(ini_get "colors.ok" "#a6e3a1") ;;
                    COLOR_MUTED)     var_value=$(ini_get "colors.muted" "#a6adc8") ;;
                    OCWS_BLUR)       var_value=$(ini_get "ocws.blur" "5") ;;
                    OCWS_BORDER)     var_value=$(ini_get "ocws.border" "1") ;;
                    OCWS_RADIUS)     var_value=$(ini_get "ocws.radius" "8") ;;
                    OCWS_SHADOW)     var_value=$(ini_get "ocws.shadow" "4") ;;
                    ICON_THEME)      var_value=$(ini_get "gtk3.icon_theme" "") ;;
                    FONT_MONO)       var_value=$(ini_get "fonts.mono" "Noto Sans Mono CJK SC:hilight=Filled") ;;
                    THEMERC_FONT)        var_value=$(ini_get "labwc.themerc_font" "sans 10") ;;
                    THEMERC_ACTIVE_BG)   var_value=$(ini_get "labwc.themerc_active_bg" "#1e1e2e") ;;
                    THEMERC_ACTIVE_TEXT) var_value=$(ini_get "labwc.themerc_active_text" "#cdd6f4") ;;
                    THEMERC_INACTIVE_BG) var_value=$(ini_get "labwc.themerc_inactive_bg" "#181825") ;;
                    THEMERC_INACTIVE_TEXT) var_value=$(ini_get "labwc.themerc_inactive_text" "#a6adc8") ;;
                    BORDER_WIDTH)        var_value=$(ini_get "labwc.border_width" "1") ;;
                    THEMERC_BORDER)      var_value=$(ini_get "labwc.themerc_border" "#45475a") ;;
                    THEMERC_HEIGHT)      var_value=$(ini_get "labwc.themerc_height" "28") ;;
                    OSD_BG)              var_value=$(ini_get "labwc.osd_bg" "#1e1e2e") ;;
                    OSD_BORDER)          var_value=$(ini_get "labwc.osd_border" "#45475a") ;;
                    OSD_TEXT)            var_value=$(ini_get "labwc.osd_text" "#cdd6f4") ;;
                    OSD_ACCENT)          var_value=$(ini_get "labwc.osd_accent" "#89b4fa") ;;
                    OSD_INACTIVE)        var_value=$(ini_get "labwc.osd_inactive" "#6c7086") ;;
                    *)
                        if [[ "$var_name" == FOOT_* ]]; then
                            local foot_key="${var_name#FOOT_}"
                            foot_key="${foot_key,,}"
                            if [[ "$foot_key" == fg ]]; then foot_key="color_foreground"; fi
                            if [[ "$foot_key" == bg ]]; then foot_key="color_background"; fi
                            if [[ "$foot_key" == cursor_fg ]]; then foot_key="color_cursor_fg"; fi
                            if [[ "$foot_key" == cursor_bg ]]; then foot_key="color_cursor_bg"; fi
                            if [[ "$foot_key" == selection_bg ]]; then foot_key="color_selection_bg"; fi
                            if [[ "$foot_key" == selection_fg ]]; then foot_key="color_selection_fg"; fi
                            if [[ "$foot_key" =~ ^regular_[0-7]$ ]]; then foot_key="color_${foot_key}"; fi
                            if [[ "$foot_key" =~ ^bright_[0-7]$ ]]; then foot_key="color_${foot_key}"; fi
                            var_value=$(ini_get "foot.$foot_key" "")
                            if [[ "$foot_key" == color_* ]]; then
                                var_value="${var_value#\#}"
                            fi
                        elif [[ "$var_name" == ROFI_* ]]; then
                            local key="${var_name#ROFI_}"
                            var_value=$(ini_get "rofi.${key,,}" "")
                        elif [[ "$var_name" == FUZZEL_* ]]; then
                            local key="${var_name#FUZZEL_}"
                            var_value=$(ini_get "fuzzel.${key,,}" "")
                        elif [[ "$var_name" == MAKO_* ]]; then
                            local key="${var_name#MAKO_}"
                            var_value=$(ini_get "mako.${key,,}" "")
                        elif [[ "$var_name" == QT_* ]]; then
                            local key="${var_name#QT_}"
                            var_value=$(ini_get "qt6ct.${key,,}" "")
                        elif [[ "$var_name" == GTK_* ]]; then
                            local key="${var_name#GTK_}"
                            if [[ "$key" == PREFER_DARK ]]; then key="application_prefer_dark_theme"; fi
                            if [[ "$key" == SHOWS_APP_MENU ]]; then key="shell_shows_app_menu"; fi
                            if [[ "$key" == SHOWS_MENU_BAR ]]; then key="shell_shows_menu_bar"; fi
                            var_value=$(ini_get "gtk3.gtk_${key,,}" "")
                            if [[ -z "$var_value" ]]; then var_value=$(ini_get "gtk3.${key,,}" ""); fi
                        elif [[ "$var_name" == XFT_* ]]; then
                            local key="${var_name,,}"
                            var_value=$(ini_get "gtk3.${key}" "")
                        elif [[ "$var_name" == FONT_* ]]; then
                            local key="${var_name#FONT_}"
                            var_value=$(ini_get "fonts.${key,,}" "")
                        elif [[ "$var_name" == CURSOR_* ]]; then
                            local key="${var_name#CURSOR_}"
                            var_value=$(ini_get "cursor.${key,,}" "")
                        elif [[ "$var_name" == COLOR_* ]]; then
                            local key="${var_name#COLOR_}"
                            var_value=$(ini_get "colors.${key,,}" "")
                        elif [[ "$var_name" == BG_ALPHA || "$var_name" == SURFACE_ALPHA || "$var_name" == BORDER_ALPHA ]]; then
                        elif [[ "$var_name" == FONT_SIZE || "$var_name" == FONT_SIZE_SMALL || "$var_name" == MODULE_* ]]; then
                            var_value=$(ini_get "sfwbar.${var_name,,}" "")
                        elif [[ "$var_name" == CORNER_RADIUS ]]; then
                            var_value=$(ini_get "labwc.cornerRadius" "8")
                        else
                            warn "Unknown template variable: {{$var_name}}"
                        fi
                        ;;
        esac

        # Always replace to prevent infinite loops on empty/unknown variables
        # Use case defaults as fallback when var_value is empty
        if [[ -z "$var_value" ]]; then
            case "$var_name" in
                COLOR_BG)      var_value="#1e1e2e" ;;
                COLOR_FG)      var_value="#cdd6f4" ;;
                COLOR_SURFACE) var_value="#1e1e2e" ;;
                COLOR_BORDER)  var_value="#45475a" ;;
                COLOR_ACCENT)  var_value="#89b4fa" ;;
                COLOR_URGENT)  var_value="#f38ba8" ;;
                COLOR_OK)      var_value="#a6e3a1" ;;
                COLOR_MUTED)   var_value="#a6adc8" ;;
                OCWS_BLUR)     var_value="5" ;;
                OCWS_BORDER)   var_value="1" ;;
                OCWS_RADIUS)   var_value="8" ;;
                OCWS_SHADOW)   var_value="4" ;;
            esac
        fi
        content="${content//\{\{$var_name\}\}/$var_value}"
    done

    echo "$content"
}

# ============================================================
# Output paths
# ============================================================

# Maps template name → install destination
declare -A OUTPUT_MAP=(
    [gtk.css.tmpl]="$HOME/.config/gtk-3.0/gtk.css"
    [gtk4.css.tmpl]="$HOME/.config/gtk-4.0/gtk.css"
    [gtk3-settings.ini.tmpl]="$HOME/.config/gtk-3.0/settings.ini"
    [gtk4-settings.ini.tmpl]="$HOME/.config/gtk-4.0/settings.ini"
    [themerc-override.tmpl]="$HOME/.config/labwc/themerc-override"
    [environment.tmpl]="$HOME/.config/labwc/environment"
    [sfwbar.css.tmpl]="$HOME/.config/ocws/theme.css"
    [tokens.css.tmpl]="$HOME/.config/ocws/tokens.css"
    [rofi.rasi.tmpl]="$HOME/.config/rofi/config.rasi"
    [fuzzel.ini.tmpl]="$HOME/.config/fuzzel/fuzzel.ini"
    [mako.ini.tmpl]="$HOME/.config/mako/config"
    [foot.ini.tmpl]="$HOME/.config/foot/foot.ini"
    [qt6ct.conf.tmpl]="$HOME/.config/qt6ct/qt6ct.conf"
    [ocws.css.tmpl]="$HOME/.config/ocws/ocws.css"
)

)

# ============================================================
# Commands
# ============================================================

cmd_list() {
    echo -e "${BOLD}Available themes:${NC}"
    echo ""
    for f in "$THEMES_DIR"/*.ini; do
        [[ -f "$f" ]] || continue
        local name desc
        # Quick parse without full INI load
        name=$(grep -m1 '^name=' "$f" 2>/dev/null | cut -d= -f2- | xargs)
        desc=$(grep -m1 '^description=' "$f" 2>/dev/null | cut -d= -f2- | xargs)
        local base
        base=$(basename "$f" .ini)
        printf "  ${CYAN}%-20s${NC} %s — %s\n" "$base" "${name:-$base}" "${desc:-}"
    done
    echo ""
}

cmd_current() {
    local current_theme="$HOME/.config/labwc/.current-theme"
    if [[ -f "$current_theme" ]]; then
        echo "Active theme: $(cat "$current_theme")"
    else
        echo "No active theme set"
    fi
}

cmd_apply() {
    local theme_file=""
    local profile="full"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --profile)
                profile="$2"
                shift 2
                ;;
            *)
                theme_file="$1"
                shift
                ;;
        esac
    done

    [[ -n "$theme_file" ]] || fail "No theme specified"
    [[ -f "$theme_file" ]] || fail "Theme not found: $theme_file"

    local theme_name
    theme_name=$(basename "$theme_file" .ini)

    echo -e "${BOLD}Applying theme: $theme_name (Profile: $profile)${NC}"
    echo ""

    # Parse and expand
    parse_ini "$theme_file"
    expand_vars

    local applied=0

    # GTK CSS (same for GTK3 and GTK4)
    mkdir -p "$HOME/.config/gtk-3.0" "$HOME/.config/gtk-4.0"
    local gtk_css
    gtk_css=$(render_template "$TEMPLATES_DIR/gtk.css.tmpl")
    if [[ -n "$gtk_css" ]]; then
        echo "$gtk_css" > "$HOME/.config/gtk-3.0/gtk.css"
        echo "$gtk_css" > "$HOME/.config/gtk-4.0/gtk.css"
        pass "gtk.css (GTK3 + GTK4)"
        applied=$((applied + 1))
    fi

    # Shared color tokens must live next to gtk.css or @ocws_* is undefined
    local gtk_tokens
    gtk_tokens=$(render_template "$TEMPLATES_DIR/tokens.css.tmpl")
    if [[ -n "$gtk_tokens" ]]; then
        echo "$gtk_tokens" > "$HOME/.config/gtk-3.0/tokens.css"
        echo "$gtk_tokens" > "$HOME/.config/gtk-4.0/tokens.css"
        pass "gtk tokens.css (GTK3 + GTK4)"
        applied=$((applied + 1))
    fi

    # GTK3 settings.ini
    local gtk3_ini
    gtk3_ini=$(render_template "$TEMPLATES_DIR/gtk3-settings.ini.tmpl")
    if [[ -n "$gtk3_ini" ]]; then
        echo "$gtk3_ini" > "$HOME/.config/gtk-3.0/settings.ini"
        pass "GTK3 settings.ini"
        applied=$((applied + 1))
    fi

    # GTK4 settings.ini
    local gtk4_ini
    gtk4_ini=$(render_template "$TEMPLATES_DIR/gtk4-settings.ini.tmpl")
    if [[ -n "$gtk4_ini" ]]; then
        echo "$gtk4_ini" > "$HOME/.config/gtk-4.0/settings.ini"
        pass "GTK4 settings.ini"
        applied=$((applied + 1))
    fi

    # Labwc themerc-override
    local themerc
    themerc=$(render_template "$TEMPLATES_DIR/themerc-override.tmpl")
    if [[ -n "$themerc" ]]; then
        echo "$themerc" > "$HOME/.config/labwc/themerc-override"
        pass "labwc themerc-override"
        applied=$((applied + 1))
    fi

    # Environment
    local environment
    environment=$(render_template "$TEMPLATES_DIR/environment.tmpl")
    if [[ -n "$environment" ]]; then
        echo "$environment" > "$HOME/.config/labwc/environment"
        pass "labwc environment"
        applied=$((applied + 1))
    fi

    # SFWBar CSS
    local sfwbar_css
    sfwbar_css=$(render_template "$TEMPLATES_DIR/sfwbar.css.tmpl")
    if [[ -n "$sfwbar_css" ]]; then
        echo "$sfwbar_css" > "$HOME/.config/ocws/theme.css"
        pass "theme.css"
        applied=$((applied + 1))
    fi

    # CSS Tokens (single source of truth for colors)
    local tokens_css
    tokens_css=$(render_template "$TEMPLATES_DIR/tokens.css.tmpl")
    if [[ -n "$tokens_css" ]]; then
        echo "$tokens_css" > "$HOME/.config/ocws/tokens.css"
        pass "tokens.css"
        applied=$((applied + 1))
    fi

    # OCWS Glass CSS
    local ocws_css
    ocws_css=$(render_template "$TEMPLATES_DIR/ocws.css.tmpl")
    if [[ -n "$ocws_css" ]]; then
        echo "$ocws_css" > "$HOME/.config/ocws/ocws.css"
        pass "ocws.css"
        applied=$((applied + 1))
    fi

    # Rofi
    local rofi_css
    rofi_css=$(render_template "$TEMPLATES_DIR/rofi.rasi.tmpl")
    if [[ -n "$rofi_css" ]]; then
        echo "$rofi_css" > "$HOME/.config/rofi/config.rasi"
        pass "rofi.rasi"
        applied=$((applied + 1))
    fi

    # Fuzzel
    local fuzzel_ini
    fuzzel_ini=$(render_template "$TEMPLATES_DIR/fuzzel.ini.tmpl")
    if [[ -n "$fuzzel_ini" ]]; then
        mkdir -p "$HOME/.config/fuzzel"
        echo "$fuzzel_ini" > "$HOME/.config/fuzzel/fuzzel.ini"
        pass "fuzzel.ini"
        applied=$((applied + 1))
    fi

    # Mako
    local mako_ini
    mako_ini=$(render_template "$TEMPLATES_DIR/mako.ini.tmpl")
    if [[ -n "$mako_ini" ]]; then
        echo "$mako_ini" > "$HOME/.config/mako/config"
        pass "mako.ini"
        applied=$((applied + 1))
    fi

    # Foot
    local foot_ini
    foot_ini=$(render_template "$TEMPLATES_DIR/foot.ini.tmpl")
    if [[ -n "$foot_ini" ]]; then
        echo "$foot_ini" > "$HOME/.config/foot/foot.ini"
        pass "foot.ini"
        applied=$((applied + 1))
    fi

    # Qt
    local qt_conf
    qt_conf=$(render_template "$TEMPLATES_DIR/qt6ct.conf.tmpl")
    if [[ -n "$qt_conf" ]]; then
        echo "$qt_conf" > "$HOME/.config/qt6ct/qt6ct.conf"
        pass "qt6ct.conf"
        applied=$((applied + 1))
    fi

    # Update widget profile if sfwbar config exists
    local ocws_config="$HOME/.config/ocws/ocws.config"
    if [[ -f "$ocws_config" ]]; then
        sed -i "s|include(\"widget-sets/.*\.set\")|include(\"widget-sets/${profile}.set\")|g" "$ocws_config"
        pass "Widget profile set to: $profile"
        applied=$((applied + 1))
    fi

    echo ""
    pass "Theme $theme_name applied successfully (${applied} files generated/updated)"
}

cmd_preview() {
    local theme_file="$1"
    [[ -f "$theme_file" ]] || fail "Theme not found: $theme_file"

    parse_ini "$theme_file"
    expand_vars

    echo -e "${BOLD}Preview for: $(basename "$theme_file" .ini)${NC}"
    echo ""

    for tmpl_file in "$TEMPLATES_DIR"/*.tmpl; do
        [[ -f "$tmpl_file" ]] || continue
        local name
        name=$(basename "$tmpl_file")
        echo "=== $name ==="
        render_template "$tmpl_file"
        echo "---"
    done
}

cmd_export() {
    local theme_file="$1"
    [[ -f "$theme_file" ]] || fail "Theme not found: $theme_file"

    parse_ini "$theme_file"
    expand_vars

    local theme_name="$(basename "$theme_file" .ini)"
    echo -e "${BOLD}Exporting theme $theme_name to dotfiles/${NC}"
    echo ""

    # Generate all files
    for tmpl_file in "$TEMPLATES_DIR"/*.tmpl; do
        [[ -f "$tmpl_file" ]] || continue
        local name
        name=$(basename "$tmpl_file")
        local content
        content=$(render_template "$tmpl_file")

        if [[ -n "$content" ]]; then
            local dest
            dest="${OUTPUT_MAP[$name]:-}"
            if [[ -n "$dest" ]]; then
                # Convert $HOME/.config to $DOTFILES_DIR
                dest="${dest/$HOME\/.config/$DOTFILES_DIR}"
                mkdir -p "$(dirname "$dest")"
                echo "$content" > "$dest"
                pass "$name → ${dest#$PROJECT_DIR/}"
            else
                warn "No output destination for $name"
            fi
        fi
    done

    # Also export tokens into the GTK dirs so gtk.css @import resolves
    if [[ -f "$TEMPLATES_DIR/tokens.css.tmpl" ]]; then
        local gtk_tokens_export
        gtk_tokens_export="$(render_template "$TEMPLATES_DIR/tokens.css.tmpl")"
        if [[ -n "$gtk_tokens_export" ]]; then
            for d in gtk-3.0 gtk-4.0; do
                local gd="$DOTFILES_DIR/$d"
                mkdir -p "$gd"
                echo "$gtk_tokens_export" > "$gd/tokens.css"
                pass "tokens.css → $d/"
            done
        fi
    fi

        fi
    fi

    info "Theme files exported to dotfiles/"
}

# ============================================================
# Profile switching
# ============================================================

cmd_profile() {
    local profile="${1:-}"
    local profiles_dir="$DOTFILES_DIR/ocws/widget-sets"

    if [[ -z "$profile" ]]; then
        echo -e "${BOLD}Available profiles:${NC}"
        echo ""
        for f in "$profiles_dir"/*.set; do
            [[ -f "$f" ]] || continue
            local name
            name=$(basename "$f" .set)
            local count
            count=$(grep -c '^include(' "$f" 2>/dev/null || echo 0)
            printf "  ${CYAN}%-15s${NC} %d widgets\n" "$name" "$count"
        done
        echo ""
        echo "Usage: $0 profile <name>"
        return
    fi

    local profile_file="$profiles_dir/${profile}.set"
    [[ -f "$profile_file" ]] || fail "Profile not found: $profile_file"

    # Symlink or copy the profile as plugins.config
    local plugins_config="$DOTFILES_DIR/ocws/plugins.config"
    cp "$profile_file" "$plugins_config"
    pass "Switched to profile: $profile ($(grep -c '^include(' "$profile_file") widgets)"
}

# ============================================================
# Main
# ============================================================

if [[ "$#" -lt 1 ]]; then
    echo "Usage: $0 {apply|preview|list|current|export|profile} [args]"
    echo ""
    echo "Commands:"
    echo "  apply <theme.ini>       Apply theme (generate + install)"
    echo "  preview <theme.ini>     Show what would be generated"
    echo "  list                    List available themes"
    echo "  current                 Show active theme"
    echo "  export <theme.ini>      Export generated files to dotfiles/"
    echo "  profile <standard|full> Switch widget set profile"
    exit 1
fi

cmd="$1"
shift

case "$cmd" in
    apply)
        cmd_apply "$@"
        ;;
    preview)
        cmd_preview "$@"
        ;;
    list)
        cmd_list
        ;;
    current)
        cmd_current
        ;;
    export)
        cmd_export "$@"
        ;;
    profile)
        cmd_profile "$@"
        ;;
    *)
        echo "Unknown command: $cmd"
        echo "Usage: $0 {apply|preview|list|current|export|profile} [args]"
        exit 1
        ;;
esac
