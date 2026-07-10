# ED Screenshot

A specialized fork of [3Dmigoto](https://github.com/bo3b/3Dmigoto) designed exclusively for **Elite Dangerous**. 

> [!WARNING]  
> This tool is built specifically for Elite Dangerous. Do not attempt to use it with other games, as it may cause instability.

## Overview

This mod enables the capture of High Dynamic Range (HDR) screenshots within Elite Dangerous. By capturing images directly from the game's rendering pipeline, this tool allows you to bypass standard game or Steam screenshots that have "baked-in" post-processing.

Depending on your configuration, images are saved in **OpenEXR** or **32-bit float TIFF** (default) formats. This provides the flexibility to adjust exposure, tone mapping, and bloom during post-processing.

### Key Features
- **HDR Capture:** Save raw lighting data for professional editing.
- **High Fidelity:** Compatible with high supersampling settings.
- **EDHM Integration:** Designed to work seamlessly with [EDHM](https://bluemystical.github.io/edhm-api/) by replacing the `d3d11.dll` file.

---

## Installation

### Option A: Using the Installer (Recommended)
1. Install [EDHM](https://bluemystical.github.io/edhm-api/).
2. Navigate to the [Latest Releases](https://github.com/Siegfried-Origin/EDScreenshot/releases/latest).
3. Download and run `EDScreenshotInstaller`.

### Option B: Manual Installation
1. Install [EDHM](https://bluemystical.github.io/edhm-api/).
2. Download the latest `ED.Screenshot.vXXX.zip` from the [Releases page](https://github.com/Siegfried-Origin/EDScreenshot/releases/latest).
3. Locate your Elite Dangerous installation folder. 
   * *Example:* `C:\Program Files (x86)\Steam\steamapps\common\Elite Dangerous\Products\elite-dangerous-odyssey-64`
4. Replace the existing `d3d11.dll` with the version provided in the ZIP file.
5. Copy `EDHM-ini\3rdPartyMods\EDScreenshot.ini` from the ZIP into your local `EDHM-ini\3rdPartyMods` folder.

---

## Usage & Configuration

### Capturing Screenshots
- **Standard HDR Capture:** Press `F12` (default).
- **Save Location:** Images are saved to `%USERPROFILE%\Pictures\ED Screenshot`.
- **Post-Processing:** 
  - `.exr` files can be edited in **GIMP** or **Affinity Photo**.
  - `.tiff` files can be edited in **Adobe Lightroom**.

### Configuration
The configuration file is located at:  
`...\Elite Dangerous\Products\elite-dangerous-odyssey-64\EDHM-ini\3rdPartyMods\EDScreenshot.ini`

| Feature | Setting | Note |
| :--- | :--- | :--- |
| **File Format** | `format = tiff` or `exr` | Change to `exr` for OpenEXR format. |
| **Standard Hotkey**| `screenshot = no_modifiers VK_F12` | Change the key assigned to capture. |
| **High-Res Mode** | `;screenshothd = ALT VK_F10` | Remove the `;` (uncomment) to enable HD captures. *Note: This is slow and impacts performance of the high resolution screen capture process.* |

---

## Additional Tips & Limitations

### Improving Quality
To increase the resolution of your captures, go to **Options → Graphics → Quality → Supersampling** in the game menu and set it to **1.0 or higher**. Because screenshots are captured at the internal rendering resolution, higher supersampling results in sharper images.

### Compatibility
This tool is compatible with the **Clean Screenshot Mod** from EDHM-UI for removing holographic UI elements.

### Known Limitations
- **Fixed Directory:** The output folder is currently hardcoded to `%USERPROFILE%\Pictures\ED Screenshot`.
- **Color Accuracy:** Images will not match Steam or native screenshots exactly. This is because Elite Dangerous applies tone mapping and blurring after the pipeline stage where this tool captures the data. However, this allows you to apply your own professional tone mapping in post-production.
