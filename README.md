# AVP2010ModelViewer
Model viewer for a game "Aliens versus Predator (2010)"
AVP2010ModelViewer is fork of AVP2010MapViewer that focuses on models, instead of map. Allow see animations.

![image](https://github.com/Trololp/AVP2010ModelViewer/blob/main/preview.png)

## Installation

Make sure your computer support DirectX 11. Download [last release v0.7.1](https://github.com/Trololp/AVP2010ModelViewer/releases/tag/v0.7.1). unpack zip files into new folder. done.
## Development
I use **Visual Studio 2017**. Used **Microsoft DirectX SDK (June 2010)**. also you need **directXTK** for text on a screen (this was get from Nuget). There file in 
[usefull_stuff](https://github.com/Trololp/AVP2010MapViewer/tree/main/usefull_stuff) folder named **HACKS.lib** dont scare its is d3dcompiler v47 lib. this is solving
my problem with linking. find and delete in `AVP2010MapViewer.h` this stroke `#pragma comment(lib, "HACKS.lib")` if all compiling good.

## Usage

  ### Preparations
   1. Download [QuickBMS](https://aluigi.altervista.org/quickbms.htm)
   2. Then download script [asura.bms](https://github.com/Trololp/AVP2010MapViewer/blob/main/usefull_stuff/asura.bms)
   3. Unpack mission archives. And select unpacked folder in this tool, wait until it loads.
 
  ### Controls   
   **Usage**
   - use keys W, S, A, D to move camera. press RMB (Right mouse button) to turn camera around.
   - key 'K' - on/off skeleton view
   - key 'B' - on/off bounding boxes view
   - key 'N' - on/off bones names.
   - key 'H' - hide HUD, and infomation.
   - use arrow keys to select animation, and model. 'left','right' select model.
   'up', 'down' arrow select animation.
   - key 'space' will pause/unpause animation
   ### Console commands
   - 'dump_anim' - debug feature. dumps selected animation into `debug.txt`
   - 'dump_model %arg%' - export model command. (format  [GLTF](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html))
   - 'dump_model all' - export model with all supported animations (rule bones == animation_bones)
   - 'dump_model only_model' - export model without animations
   - 'dump_model only_current_anim' - export model with selected animation.
   ### log file
   - this file called 'debug.txt' and exist in application folder
