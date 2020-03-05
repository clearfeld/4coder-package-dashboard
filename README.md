# 4coder-package-dashboard

An extensible 4coder startup screen.

## Features

## Configuration

## Installation
```cpp
// Include the dashboard cpp file within your custom_layer
#include "../packages/dashboard/dashboard.cpp"

// then add the following at the end of your custom_render_buffer function
if(buffer == dashboard_buffer_id) {
    draw_dashboard_extras(app, text_layout_id);
}
```

If you want the dashboard to open on 4coder startup, you'll need to add the following as well.
```cpp
// Add the following funcion to your custom startup (taken from default_startup)
dashboard_open(app);

// and replace the BindCore with your custom startup within your keybinding layer
//BindCore(default_startup, CoreCode_Startup);
BindCore(/* your custom startup function*/, CoreCode_Startup);
```

The dashboard view by default will look within the root 4ed.exe folder for the dashboard folder.
Within this folder it will automatically load the 4 named files:

1. agenda
2. bookmarks
3. project
4. recent

## Commands
| Command        | Action                                                  |
| -------------- | ------------------------------------------------------- |
| dashboard_open | Opens the dashboard buffer in the currently active view |

## Shortcuts
| Shortcut  | Action                                        |
| --------- | --------------------------------------------- |
| Return    | Try to open file or load project under cursor |
| r         | Jump to first item under recent files group   |
| m         | Jump to first item under bookmarks group      |
| p         | Jump to first item under project group        |
| a         | Jump to first item under agenda group         |

## Contributions

To contribute your changes to this package, please do the following:

1. Fork the repo
2. Clone a local copy
3. Make your changes
4. Push and create your PR
