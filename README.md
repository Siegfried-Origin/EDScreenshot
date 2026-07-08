# ED HDR Screenshot

This repository is a fork of 3Dmigoto specifically designed for Elite Dangerous.

Do not attempt to use it with other games.

If you are looking for the original 3Dmigoto project, please download it from:
https://github.com/bo3b/3Dmigoto/releases

## Capture HDR Screenshots in Elite Dangerous

This mod enables HDR screenshot capture in Elite Dangerous.

When you press **F12** (configurable), the tool captures an HDR image from the game's rendering pipeline. Depending on the format specified in the configuration, it is saved in **OpenEXR** format or 32bit float **TIFF** format (default), allowing you to adjust exposure, tone mapping, bloom, and other post-processing effects that are normally baked into standard game or Steam screenshots.

The images are saved in the **ED Screenshot** subdirectory of your Pictures folder.

You can edit the resulting EXR files with software such as **GIMP** or **Affinity Photo** and TIFF files in **Adobe Lightroom**.

This tool is designed to be compatible with EDHM by replacing the `d3d11.dll` file used by EDHM: https://bluemystical.github.io/edhm-api/

Although the tool can run without EDHM, installing EDHM is strongly recommended because:

1. EDHM is an outstanding tool.
2. The installation instructions below assume that EDHM is already installed.

## Installation
1. Install EDHM: https://bluemystical.github.io/edhm-api/
2. Get to the latest release: https://github.com/Siegfried-Origin/ED-HDR-Screenshot/releases/latest
3. Download the ZIP file `ED.HDR.Screenshot.vXXX.zip` from the list.
4. Navigate to your Elite Dangerous installation folder (for example: `C:\Program Files (x86)\Steam\steamapps\common\Elite Dangerous\Products\elite-dangerous-odyssey-64`).
5. Replace the existing `d3d11.dll` with the one included in the ZIP file.
6. Copy `EDHM-ini\3rdPartyMods\EDScreenshot.ini` from the ZIP file to your `EDHM-ini\3rdPartyMods` folder.

You can now press `F12` to capture an HDR screenshot.

The captured images will be saved in the `ED Screenshot` subfolder of your user's `Pictures` folder.

## Additional Information

* In the game's main menu, ensure that **Options → Graphics → Quality → Supersampling** is set to **1.0 or higher**. Screenshots are captured at the internal rendering resolution, so increasing supersampling can be used to generate higher-resolution captures, even in Open Play.
* This tool can be used together with the **Clean Screenshot Mod** from EDHM-UI if you want to remove holographic UI elements from your screenshots.

## Limitations

* High-resolution screenshots taken with **Alt+F10** are not supported.

* The output directory is currently fixed to:

  ```
  %USERPROFILE%\Pictures\ED Screenshot
  ```

* Colors will not match Steam or native in-game screenshots exactly. This is expected: Elite Dangerous composites the blurred image and applies tone mapping and other post-processing effects before producing the final image.

  The original HDR data is preserved in the EXR files, allowing you to reproduce, or improve upon, the game's final appearance during post-processing.

  Support for extracting the game's color LUTs and a more complete editing guide may be added in a future release.
