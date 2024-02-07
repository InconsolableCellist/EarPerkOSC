# 🎧 EarPerkOSC

**EarPerkOSC** is a Rust application that listens to stereo audio from an input source and sends OSC messages to 
make your VRChat avatar's ears perk. If a loud sound occurs mostly on your left side, the message to perk the left ear
will be sent, same for the right, and same for audio that's close to the center. If the audio is especially loud, your ears
can also fold back protectively!

After a delay (configurable), the message to unperk your ears may be sent (if the sound has stopped). Boolean VRCExpressionParameters are used to 
control the ear perking, and to only require 2 to 4 bits of space in your parameters. 
(Floats may be a future improvement, for intensity of the perk and possibly directionality.)

Audio is captured from your system's audio as a loopback device--so anything you hear, your avatar will react to. (Including Discord/Telegram
notification pings.)

## 🛰️ OSC Configuration 

An OSC connection is made to 127.0.0.1:9000 by default. If for some reason you need a custom endpoint, you can configure it in the config.ini file.

**EarPerkOSC** works with other OSC applications, like [VRCFT](https://github.com/benaclejames/VRCFaceTracking) and it doesn't require any special OSC routing.

## 🦊 Avatar configuration

You'll need to modify your avatar to respond to updates to the avatar parameters. EarPerkLeft and EarPerkRight booleans
at a minimum. You can use my template with [VRCFury](https://vrcfury.com/) to do this quite easily.

My template is available on my Gumroad: https://foxipso.gumroad.com

If you're endeavoring to do this yourself, make the following animation files:

* Ear Left Perk
* Ear Left Neutral
* Ear Right Perk
* Ear Right Neutral
* Both Ears Fold

Then create an animation controller using the avatar parameters as floats (they're bools in the VRCExpressionParameters, but you use them as if they were floats).
You can animate them using direct and 1D blend trees. The animation clips don't need more than one keyframe.

## ⚙️ config.ini

Your `config.ini` file will automatically be created when you run the binary for the first time.
Here's a complete config.ini file an explanation of the configurable parameters:

```ini
encoding=utf-8

[version]
version=1.0

[connection]
address=127.0.0.1
port=9000
osc_address_left=/avatar/parameters/EarPerkLeft
osc_address_right=/avatar/parameters/EarPerkRight

[audio]
input_device=CABLE Output (VB-Audio Virtual Cable)
differential_threshold=0.01
reset_timeout_ms=1000
timeout_ms=100
buffer_size_ms=100
```

* `address` and `port` are the address and port of the OSC server you're sending to (VRChat or VOR)

* `osc_address_left` and `osc_address_right` are the OSC addresses for the left and right ear parameters on your avatar. You shouldn't need to change these, but they correspond to your VRCExpressionParameter entries
*  `input_device` is the name of the audio device you want to capture. You can find this by running the program and looking at the output.
* `differential_threshold` is the minimum difference between the left and right channels to trigger a left or right-only ear perk.
* `reset_timeout_ms` is the time in milliseconds to wait after the last sound before sending the unperk message, if applicable.
* `timeout_ms` is the time in milliseconds to wait before trying to perk that ear again
* `buffer_size_ms` is the size of the buffer in milliseconds. This is the amount of audio data that'll be averaged together before being processed.

**The important configuration parameters** for most setups are `port`, `input_device`, and to a lesser extent `differential_threshold`, though
the default value works just fine for me.


## 💾 Installation

Download the provided latest release, extract it to a directory, and run `EarPerkOSC.exe`. If you need to make
configuration changes, open the newly created `config.ini` file. 

Unless you're using Voice Meeter, you'll need to change which audio device is captured. 
Look at the output in the console and find the name of the device you want to capture. Place
that in the config.ini. See the Configuration Section for more details.

You'll see explanatory text in the program's console output. As audio is processed and messages are sent, you'll see the following:

`L`: OSC message sent to the left ear address, as a True 

`R`: OSC message sent to the right ear address, as a True

`B`: OSC message sent individually to left and right, as a True

`!L`: OSC Message sent to the left ear address, as a False, to unperk the ear

`!R`: OSC Message sent to the right ear address, as a False, to unperk the ear


## 🛠️ Building 

1. Clone the repository
2. Navigate to the project directory
3. Run `cargo build` to build the project
4. Run `cargo run` to start the application

## 🤝 Support

For support, please visit [foxipso.com](http://foxipso.com)

## 📅 Version History 

1.0.0 - Initial release


## ⚖️ License

This project is licensed under the ???
