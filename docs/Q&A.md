# Q&A / About The Project

Some imaginary questions and answers on this project, you know just me talking to myself and all... Feel free to ask more, I can extend this list over time if need be!

### Why work on this project? Are you insane?
Maybe, but here are a couple of my reasons anyway:

- For experience/learning purposes. I had been reading Fabien Sanglard's excellent _GAME ENGINE BLACK BOOK: DOOM_ recently and wanted to put some of the more technical aspects of the book to practical use. Actually working on the code has taught me a lot more than just reading alone.
- I had found out about the source release of 3DO Doom a while ago and was intrigued by the chaotic story of it's development and the potential (sadly squandered) that it had to be one of the best console ports of DOOM instead of one of the worst (to paraphrase Rebecca Ann Heineman, it's developer). Thus this remaster is kind of a 'what if' type scenario - maybe given much more development time (instead of an insane 10 weeks!) and better hardware (3DO M2, anyone?) the game could have been more in this sort of direction rather than what actually happened.
- Unlike other console DOOM source code releases, I didn't see any attempts to backport this to the PC yet (like say [Calico](https://github.com/team-eternity/calico-doom) in the case of Jaguar DOOM) even though there have been efforts to improve the game running on the original hardware (See: [OptiDoom](https://github.com/Optimus6128/optidoom3do)). Thus I figured this might be a good opportunity to fill in a small gap in the list of DOOM source ports.
- The technical challenges involved in reviving a game built for a long dead/defunct platform proved alluring. I had never attempted such a project before so it felt like it might be a fun undertaking. Reverse engineering certain stuff and filling in the blanks was a very interesting challenge!
- A certain part of me likes going in and fixing up + polishing old projects. Some of the most satisfying projects I have been involved with in the past have involved this kind of work. This felt like something similar.
- DOOM is one of my favorite games of all time and arguably the reason why I'm a game developer today - having had my interest in game modding and subsequently development stoked in the early days by map editors like DEU and DCK. Thus it is fair to say this game has had a substantial impact on my life. As a result of this I wanted to contribute something small back to the DOOM community and the preservation of it's history.
- I was interested in the educational potential of this project, being a former student of a Games Development course. This is a very simplified and bare-bones version of DOOM that lacks certain complex features such networking, demo recording, WAD file management and so forth. It's the pure essence of the game in a highly distilled form and thus makes an excellent version of the game to study and start tinkering with. It is also one of the best documented and well commented versions of the DOOM I have seen, most likely thanks to Rebecca Ann Heineman's work on the game.
- I'm about to become a father soon and this was a nice smaller project with a more limited scope that I could timebox and finish up before then. At the time of writing my wife is just about due, and I've just finished up - so it worked out nicely in that regard!
- Lastly, ['It runs DOOM'](https://itrunsdoom.tumblr.com) REALLY needs another entry. It really does...

### Does/will the game support modding and/or user content?
No, this was not a priority for a number of reasons:

- The data formats used by the game are VERY different to PC DOOM. Level data is stored in big endian format (as opposed to little endian on PC) for example and there are no WAD files at all. In order to be able to support modding you would need to recreate most of the toolchain that was used to build the original game data. Some of that toolchain relies on functionality and utilities present on long defunct machines such as the original 68K Mac. Recreating all this stuff would be quite a large undertaking for what is a very niche version of DOOM!
- Modern source ports of DOOM offer much better platforms for modding anyway, with scripting languages and other nice engine enhancements available. If you want to mod DOOM, a modern source port is really the best place to do it.
- Distributing modifications/patches to a game released on a console is a headache and might carry legal issues. Again, modding the PC version of the game is the safest and cleanest route to go.

### Does/will the game support multiplayer?
No, this was not present in 3DO DOOM and was not a priority for this project. It is always possible that it could be added but I did not see the value in this given the niche status of this port.

### The game runs slow at high resolutions, why is this?
The game is not using hardware rendering (see below), which means it can be quite slow at higher resolutions. I also did not have a lot of time left at the end to optimize and try to speed things up (kind of ironic when you think of it, given the source subject), so perhaps there is room for improvement. Hint: the light diminishing effect I am doing is _VERY_ expensive, but damn... I like the results! Maybe there's a faster way to achieve a similar effect though... Lots of the rendering could also be multi-threaded too, but after that point I figure you might as well go the whole hog and implement a hardware rasterizer anyway. If you _TRULY_ want absolute speed, hardware acceleration is the way to go.

### Why does the game not use hardware accelerated rendering?
I'm aware of the downsides of not doing this (speed primarily), however I have a few reasons:
- Using a software renderer makes the game _HIGHLY_ portable, in keeping with the tradition of the original DOOM engine. Pretty much all the game needs to operate is a pixel surface to draw to, an audio stream to dump samples to and some inputs from the user for controls - that is all. Not having to juggle multiple graphics APIs and shader formats etc. keeps things nice and simple.
- Most people are probably going to want to run this at very retro looking resolutions like the original 320x200 or 640x400 ([Crispy Doom](https://github.com/fabiangreffrath/crispy-doom) style resolution) etc. anyway for a more authentic feel. A software renderer is more than fast enough for that purpose, even with my lack of optimizations.
- Using a software renderer makes it easier to preserve the original sprite clipping behavior of DOOM, which I feel is essential to the look of the game given how heavily it relies on the use of sprites. DOOM predates the use of hardware z-buffers on graphics cards and thus has unusual ways of clipping it's sprites that are not based on depth from the viewer. This allows monsters and weapon items etc. to extend into walls without being cut off and clipped. I wanted to preserve this behavior, and a software renderer was the easiest way to do this.
- The educational aspect of being able to see a software renderer manipulating pixels directly has a certain value I believe in this day and age. A lot of low level blitting operations are pretty much invisible/hidden to the graphics programmer in the modern era, so I felt there was a certain value in being able to see these things again, tweak them and re-learn the old ways of doing graphics.

### Why is this source under the MIT license rather than GPL (like PC DOOM)?
Primarily for reasons of consistency since the original [3DO Doom Source Release](https://github.com/Olde-Skuul/doom3do) is also released under the MIT license. I also don't want to dissuade people if somebody _DOES_ for some reason want to make an original retro-style FPS based on this version of the engine - that is probably a game I would like to play, assuming it's good! Please be aware however that such a project [cannot contain ANY TRACE of DOOM or it's IP](docs/License.md) - it **must** be entirely your own game concept and with your own game assets.

### Will you add support for platform X, compiler Y, build system Z etc.?
Probably not, but I'm more than happy to accept pull requests if you want to extend the range of platforms supported or make fixes for particular platforms etc. Since I'm using SDL to handle platform specific stuff, you should be able to easily support any platform that SDL already does.

### The game looks/feels somewhat different to 3DO Doom, why is that?
For this project I deliberately chose NOT to go the [Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom) or [Calico Doom](https://github.com/team-eternity/calico-doom) route of absolute authenticity and instead treated this project as more of a remastered version of the original. There were a couple of reasons why I decided upon this route:
- Emulating the original 3DO CEL engine hardware would likely be a very complex undertaking, given the lack of documentation on the system and it's relative obscurity. It didn't seem worth the effort.
- Some of the visual artifacts of 3DO DOOM like the 'fuzzy' looking texturing started to irritate me after a while and I just wanted to fix them. The original 3DO lighting also bothered me and I felt like it killed a lot of the atmosphere of the game from the PC original - again I wanted to fix this.
- I wanted to support much higher resolutions than were possible in the original game, which necessitated pretty much a complete rewrite of the rendering engine. While I was at this I figured I would try and improve some of the quality of the output.
- Lastly, I reasoned that if people want to experience the original 3DO version in all it's glory (terribleness?!) anyway then they can either run the game on the original hardware or in an emulator for a much more authentic experience. Since this use case was pretty much covered already, I didn't see the need for another way to do that.

### I noticed all the game code appears to be in C++, I thought DOOM was originally written in C?
It was, but I am a C++ programmer at heart and wanted to use some of the nice features of modern C++ such as templates, RAII, constexpr and so forth. As a result I converted the codebase over to C++. C++17 is also required because some of these goodies are only available in the latest stable version of the standard.
