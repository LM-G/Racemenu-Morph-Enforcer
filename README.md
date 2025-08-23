# Racemenu Morph Fixer

C++ SKSE plugin for Skyrim.

---

- [SKSE "Hello, world!"](#skse-hello-world)
- [What does it do?](#what-does-it-do)
- [CommonLibSSE NG](#commonlibsse-ng)
- [Requirements](#requirements)
    - [Opening the project](#opening-the-project)
- [Project setup](#project-setup)
    - [Finding Your "`mods`" Folder](#finding-your-mods-folder)
- [Setup your own repository](#setup-your-own-repository)
- [Sharing is Caring](#sharing-is-caring)

# What does it do?

Forces the racemenu morphs to be applied onto the player.

# Requirements

Edit your system or user Environment Variables and add a new one:

- Name: `VCPKG_ROOT`
- Value: `C:\path\to\wherever\your\vcpkg\folder\is`

<img src="https://raw.githubusercontent.com/SkyrimDev/Images/main/images/screenshots/Setting%20Environment%20Variables/VCPKG_ROOT.png" height="150">

## Opening the project

Once you have Visual Studio 2022 installed, you can open this folder in basically any C++ editor,
e.g. [VS Code](https://code.visualstudio.com/) or [CLion](https://www.jetbrains.com/clion/)
or [Visual Studio](https://visualstudio.microsoft.com/)

- > _for VS Code, if you are not automatically prompted to install
  the [C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
  and [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extensions, please
  install those and then close VS Code and then open this project as a folder in VS Code_

You may need to click `OK` on a few windows, but the project should automatically run CMake!

It will _automatically_ download [CommonLibSSE NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) and everything you
need to get started making your new plugin!

# Project setup

By default, when this project compiles it will output a `.dll` for your SKSE plugin into the `build/` folder.

If you want to configure this project to output your plugin files
into your Skyrim Special Edition's "`Data`" folder:

- Set the `SKYRIM_FOLDER` environment variable to the path of your Skyrim installation  
  e.g. `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`

<img src="https://raw.githubusercontent.com/SkyrimDev/Images/main/images/screenshots/Setting%20Environment%20Variables/SKYRIM_FOLDER.png" height="150">

If you want to configure this project to output your plugin files
into your "`mods`" folder:  
(_for Mod Organizer 2 or Vortex_)

- Set the `SKYRIM_MODS_FOLDER` environment variable to the path of your mods folder:  
  e.g. `C:\Users\<user>\AppData\Local\ModOrganizer\Skyrim Special Edition\mods`  
  e.g. `C:\Users\<user>\AppData\Roaming\Vortex\skyrimse\mods`

<img src="https://raw.githubusercontent.com/SkyrimDev/Images/main/images/screenshots/Setting%20Environment%20Variables/SKYRIM_MODS_FOLDER.png" height="150">

## Finding Your "`mods`" Folder

In Mod Organizer 2:

> Click the `...` next to "Mods" to get the full folder path

<img src="https://raw.githubusercontent.com/SkyrimDev/Images/main/images/screenshots/MO2/MO2SettingsModsFolder.png" height="150">

In Vortex:

<img src="https://raw.githubusercontent.com/SkyrimDev/Images/main/images/screenshots/Vortex/VortexSettingsModsFolder.png" height="150">

---

> 📜 Made possible thanks to templates found here https://github.com/SkyrimScripting/SKSE_Templates