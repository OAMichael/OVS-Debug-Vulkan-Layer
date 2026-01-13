# OVS Debug Vulkan Layer

## Usage

### First, one need to set environment variables `VK_INSTANCE_LAYERS`, `VK_LAYER_PATH` and `VK_OVS_DEBUG_SETTINGS_PATH`
#### Example:

```
$env:VK_INSTANCE_LAYERS="VK_LAYER_OVS_DEBUG"
$env:VK_LAYER_PATH="C:\OVS-Debug-Vulkan-Layer\build"
$env:VK_OVS_DEBUG_SETTINGS_PATH="C:\OVS-Debug-Vulkan-Layer\Json\OVS_DEBUG_settings.json"
```

### Second, run an executable
#### Example:

```
vkcube.exe
```