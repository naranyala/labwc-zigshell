-- Shared plugins loaded for ALL modes (universal dependencies & base setups)
-- Extracted from per-mode duplication. Individual modes may still re-declare
-- the same plugin (mode specs are merged AFTER shared, so the mode wins on
-- any conflict). Keep this list to plugins that are effectively identical
-- across every mode; anything customized per-mode stays in the mode file.
return {
  -- Icons
  { "echasnovski/mini.icons", config = function() require("mini.icons").setup() end },
  { "nvim-tree/nvim-web-devicons" },

  -- Core dependencies
  { "nvim-lua/plenary.nvim" },

  -- Editor essentials
  { "echasnovski/mini.comment", config = function() require("mini.comment").setup() end },
  { "echasnovski/mini.statusline", config = function()
    require("mini.statusline").setup({ use_icons = true, set_vim_settings = false })
  end },

  -- LSP tooling base. Modes add mason-lspconfig `ensure_installed`, the
  -- mason-tool-installer, and their own `vim.lsp.enable({...})` lists.
  { "williamboman/mason.nvim", "williamboman/mason-lspconfig.nvim" },
  { "neovim/nvim-lspconfig" },

  -- Which-key (loaded for all modes)
  { "folke/which-key.nvim", event = "VeryLazy",
    opts = { plugins = { spelling = { enabled = true } } },
  },

  -- Colorscheme is loaded separately via plugins.colorscheme
}
