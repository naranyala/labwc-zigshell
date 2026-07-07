# OCWS LLM Runner

Local-first LLM chat and OCR assistant for the OCWS desktop shell.

---

## Overview

`ocws-llm-runner` combines:

- **Local LLM Chat**: Chat with local GGUF models via `llama-cpp-python`
- **OCR Integration**: Capture screen regions or OCR image files
- **Model Management**: Load, eject, switch, download models
- **Session Manager**: Multiple conversations with persistence
- **GTK3 GUI**: Glassmorphic interface matching OCWS theme
- **REST API**: Full API for headless/scripted usage

---

## Architecture

```
ocws-llm-runner/
├── main.py              # Entry point (GUI + server)
├── requirements.txt     # Python dependencies
├── ocws-llm-runner     # Launcher script
├── server/
│   ├── app.py           # Flask REST API
│   ├── llm.py           # LLM inference engine
│   ├── ocr.py           # OCR processor
│   └── sessions.py      # Session manager
├── gui/
│   └── app.py           # GTK3 GUI
└── utils/
    └── __init__.py
```

---

## Installation

```bash
# Install Python dependencies
pip install -r src/ocws-llm-runner/requirements.txt

# For OCR support
sudo apt install tesseract-ocr

# The installer deploys the launcher and .desktop file
./install.sh
```

---

## Usage

```bash
# Start with GUI + server
ocws-llm-runner

# Start server only (headless)
ocws-llm-runner --server-only

# Start GUI only (connect to existing server)
ocws-llm-runner --gui-only

# OCR a single image
ocws-llm-runner --ocr image.png

# Specify model on startup
ocws-llm-runner --model ~/.local/share/ocws/models/model.gguf
```

---

## GUI Features

### Header Bar
- **Server Status**: Green/red indicator
- **Start/Stop**: Control server from GUI
- **Refresh**: Update all status info

### Left Sidebar
- **Sessions**: Create, switch, rename, delete, export sessions
- **Model Management**: Load, eject, switch models
- **Quick Switch**: Dropdown for downloaded models
- **System Prompt**: Edit AI system prompt

### Chat Area
- **Messages**: User (blue), Assistant (gray), System (yellow)
- **Input**: Enter to send, Shift+Enter for newline
- **OCR Button**: Quick screen region capture

---

## Model Management

### Load a Model
```bash
# Via GUI
Click "Load" → Select .gguf file

# Via API
curl -X POST http://127.0.0.1:5000/api/model/load \
  -d '{"path": "~/.local/share/ocws/models/model.gguf"}'
```

### Switch Models
```bash
# Via GUI
Click model dropdown → Select model

# Via API
curl -X POST http://127.0.0.1:5000/api/model/switch \
  -d '{"path": "/path/to/other-model.gguf"}'
```

### Eject Model
```bash
# Via GUI
Click "Eject"

# Via API
curl -X POST http://127.0.0.1:5000/api/model/eject
```

---

## Recommended Models

### For Coding

| Model | RAM | Best For |
|-------|-----|----------|
| Qwen2.5-Coder-1.5B | ~2GB | Code completion, debugging |
| DeepSeek-Coder-V2-Lite | ~3GB | Code generation, refactoring |
| Phi-3-mini-4k | ~4GB | General coding, reasoning |

### For OCR/Vision

| Model | RAM | Best For |
|-------|-----|----------|
| Llama-3.2-1B-Vision | ~2GB | Image understanding |
| LLaVA-1.6-7B | ~6GB | Visual QA, screenshots |

### Lightweight

| Model | RAM | Best For |
|-------|-----|----------|
| TinyLlama-1.1B | ~1GB | Quick tests, low RAM |

### Download

```bash
# Create model directory
mkdir -p ~/.local/share/ocws/models

# Download Qwen2.5-Coder (recommended for coding)
wget -O ~/.local/share/ocws/models/qwen2.5-coder-1.5b.gguf \
  https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF/resolve/main/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf
```

---

## Session Manager

Sessions persist conversations across restarts.

### Via GUI
- **New Session**: Click "+ New Session"
- **Switch**: Click session in list
- **Rename**: Right-click → Rename
- **Delete**: Right-click → Delete
- **Export**: Right-click → Export as JSON

### Via API
```bash
# List sessions
curl http://127.0.0.1:5000/api/sessions

# Create session
curl -X POST http://127.0.0.1:5000/api/sessions \
  -d '{"name": "My Session"}'

# Get session history
curl http://127.0.0.1:5000/api/sessions/<id>/history

# Delete session
curl -X DELETE http://127.0.0.1:5000/api/sessions/<id>
```

---

## OCR Integration

### Screen Region Capture
```bash
# Via GUI
Click "OCR" button → Select region

# Via API
curl -X POST http://127.0.0.1:5000/api/ocr/region
```

### File OCR
```bash
# Via GUI
Sidebar → "OCR from File" → Select image

# Via API
curl -X POST http://127.0.0.1:5000/api/ocr \
  -F "image=@screenshot.png" \
  -F "lang=eng"
```

### Command Line
```bash
# Using ocws-llm-runner
ocws-llm-runner --ocr image.png

# Using ocws-ocr directly
ocws-ocr                    # Capture region
ocws-ocr -c                 # Capture to clipboard
ocws-ocr screenshot.png     # OCR file
```

---

## REST API Reference

### Health & Status
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/health` | GET | Health check |
| `/api/status` | GET | Detailed server status |

### Model Management
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/models` | GET | List available models |
| `/api/model/load` | POST | Load a GGUF model |
| `/api/model/eject` | POST | Unload current model |
| `/api/model/switch` | POST | Switch to different model |
| `/api/model/download` | POST | Download model from URL |
| `/api/model/delete` | POST | Delete a model file |

### Chat
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/chat` | POST | Send message |
| `/api/chat/stream` | POST | Stream response (SSE) |

### Sessions
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/sessions` | GET | List all sessions |
| `/api/sessions` | POST | Create new session |
| `/api/sessions/<id>` | GET | Get session details |
| `/api/sessions/<id>` | PUT | Update session name |
| `/api/sessions/<id>` | DELETE | Delete session |
| `/api/sessions/<id>/history` | GET | Get chat history |
| `/api/sessions/<id>/history` | DELETE | Clear history |
| `/api/sessions/active` | GET | Get active session |
| `/api/sessions/active` | PUT | Set active session |

### OCR
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/ocr` | POST | OCR uploaded image |
| `/api/ocr/region` | POST | Capture region and OCR |

### Export/Import
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/export/session/<id>` | GET | Export session as JSON |
| `/api/import/session` | POST | Import session from JSON |

---

## Data Storage

```
~/.local/share/ocws/llm-runner/
├── meta.json           # Active session, last model
├── sessions/           # Session files
│   ├── abc123.json
│   └── def456.json
└── models/             # Downloaded models
    └── model.gguf
```

---

## Troubleshooting

### Server won't start
```bash
# Check if port is in use
lsof -i :5000

# Try different port
ocws-llm-runner --port 5001
```

### Model fails to load
```bash
# Check file exists
ls -la ~/.local/share/ocws/models/

# Check available RAM
free -h

# Try smaller model
# Qwen2.5-Coder-1.5B needs ~2GB RAM
```

### OCR not working
```bash
# Check tesseract installed
tesseract --version

# Check screenshot tools
which grim slurp

# Test directly
ocws-ocr
```
