# How To Play & Configuring

## Requirements
Phoenix DOOM requires the original 3DO DOOM game data in order to play. In order to provide this, you must first create a raw image (.IMG file) of the original 3DO Doom CD-ROM and copy this onto your computer. Once this is done, configure the game to reference this CD image (see below).

## Specifying The Game Data Location
On Windows most of the time you can simply name your 3DO Doom image file `Doom3DO.img` and place it in the same folder as the .exe (game executable) in order for it be recognized, as this what the program will look for by default.

On MacOS and Linux you will need to manually configure the path to this image file by editing the game's configuration .ini file. To do this, first launch the game as *the configuration file is generated the first time you run the app*. After first launch this file can normally be found at the following locations on Windows, MacOS and Linux:

- Windows: `C:\Users\<MY_USERNAME>\AppData\Roaming\com.codelobster\phoenix_doom`
- MacOS: `/Users/<MY_USERNAME>/Library/Application Support/com.codelobster/phoenix_doom`
- Linux: `/home/<MY_USERNAME>/.local/share/com.codelobster/phoenix_doom`

Edit the following entry and input the full path to the .IMG file to specify where the game's CD image can be found:

`CDImagePath = Doom3DO.img`

## Configuring The Game
Edit the .ini file described above to configure various aspects of the game such as render resolution, mouse sensitivity and controls. The .ini file will contain comments describing what each setting does. By default the game is pre-configured for standard WASD controls, 320x200 render resolution (scaled up to the current screen resolution) and gamepad controls optimized for an XBOX One or 360 controller.
