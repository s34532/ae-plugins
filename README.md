# sau-kit

A growing collection of native Adobe After Effects plugins. Each plugin lives in its own folder with
its source, PiPL resources, and a ready-to-install Windows `.aex` build.

## Plugins

| Plugin | Description | Files |
| --- | --- | --- |
| Volumetric Cloud Shader | Volumetric weather, native AE camera support, continuous day cycles, and above-cloud views. | [`plugins/VolumetricCloudShader`](plugins/VolumetricCloudShader) |
| ASCII Art Converter | System-font ASCII rendering with typography, directional glyph mapping, and character animation. | [`plugins/ASCIIArtConverter`](plugins/ASCIIArtConverter) |

## Install

1. Close After Effects.
2. Copy the desired `.aex` file into the After Effects `Support Files/Plug-ins` directory.
3. Start After Effects and find the effect in the `saumotion` category.

The checked-in binaries target 64-bit Windows. Building from source requires the Adobe After Effects SDK.

## Repository layout

```text
plugins/
  ASCIIArtConverter/
  VolumetricCloudShader/
```

Additional plugins should follow the same one-folder-per-plugin layout.
