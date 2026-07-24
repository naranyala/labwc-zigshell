-- ═══════════════════════════════════════════════════════════════
-- Router: loads only the active mode's plugins
-- The mode-switcher plugin manages which mode is active
-- ═══════════════════════════════════════════════════════════════

-- State file path (must match mode-switcher)
local state_file = vim.fn.stdpath("data") .. "/mode-switcher.json"

-- Read saved mode from disk
local function get_active_mode()
  local f = io.open(state_file, "r")
  if not f then return "full-ide" end
  local content = f:read("*a")
  f:close()

  local ok, data = pcall(vim.json.decode, content)
  if ok and data and data.mode then
    -- Validate mode exists
    local modes_dir = vim.fn.stdpath("config") .. "/lua/modes"
    local file = modes_dir .. "/" .. data.mode .. ".lua"
    local stat = vim.loop.fs_stat(file)
    if stat then return data.mode end
  end

  return "full-ide"
end

-- Load the active mode
local function load_mode(mode)
  local ok, mode_plugins = pcall(require, "modes." .. mode)
  if not ok then
    vim.notify(
      string.format("[mode-switcher] Failed to load mode '%s': %s", mode, mode_plugins),
      vim.log.levels.ERROR
    )
    -- Fallback to full-ide
    if mode ~= "full-ide" then
      return load_mode("full-ide")
    end
    return {}
  end
  return mode_plugins
end

-- Always load shared plugins (colorscheme, etc.)
local function load_shared_plugins()
  local shared = {}

  -- Colorscheme (always loaded)
  local ok, colorscheme = pcall(require, "plugins.colorscheme")
  if ok and colorscheme then
    for _, plugin in ipairs(colorscheme) do
      table.insert(shared, plugin)
    end
  end

  -- Shared plugins
  local ok_shared, shared_plugins = pcall(require, "plugins.shared")
  if ok_shared and shared_plugins then
    for _, plugin in ipairs(shared_plugins) do
      table.insert(shared, plugin)
    end
  end

  return shared
end

-- Main: return only the active mode's plugins
local mode = get_active_mode()
local plugins = load_mode(mode)

-- Merge shared + mode plugins
local shared = load_shared_plugins()
for _, plugin in ipairs(shared) do
  table.insert(plugins, plugin)
end

-- Log which mode is loaded
vim.api.nvim_create_autocmd("User", {
  pattern = "VeryLazy",
  once = true,
  callback = function()
    vim.notify(
      string.format("[mode-switcher] Active mode: %s (%d plugins)", mode, #plugins),
      vim.log.levels.INFO
    )
  end,
})

return plugins
