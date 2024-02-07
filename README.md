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
osc_address_overwhelmingly_loud=/avatar/parameters/EarOverwhelm

[audio]
differential_threshold=0.01
volume_threshold=0.1
excessive_volume_threshold=0.4
reset_timeout_ms=500
timeout_ms=100
```

* `address` and `port` are the address and port of the OSC server you're sending to (VRChat)

* `osc_address_left` and `osc_address_right` are the OSC addresses for the left and right ear parameters on your avatar.
* `osc_address_overwhelmingly_loud` is the OSC address for the "overwhelmingly loud" parameter. This is used to fold your ears back when the audio is too loud.
* `differential_threshold` is the minimum difference between the left and right channels to trigger a left or right-only ear perk.
* `reset_timeout_ms` is the time in milliseconds to wait after the last sound before sending the unperk message, if applicable. Low values will make your ears twitchy.
* `timeout_ms` is the delay in milliseconds to wait before trying to perk that ear again

Important configuration parameters are `volume_threshold` and `excessive_volume_threshold`, though the default values should work fine.


## 💾 Installation

Download the provided latest release, extract it to a directory, and run `EarPerkOSC.exe`. If you need to make
configuration changes, open the newly created `config.ini` file. 

You'll see explanatory text in the program's console output. As audio is processed and messages are sent, you'll see the following:

`L`: OSC message sent to the left ear address, as a True 

`R`: OSC message sent to the right ear address, as a True

`B`: OSC message sent individually to left and right, as a True

`!L`: OSC Message sent to the left ear address, as a False, to unperk the ear

`!R`: OSC Message sent to the right ear address, as a False, to unperk the ear

`O`: OSC Message sent to the "overwhelmingly loud" address, as a True

`!O`: OSC Message sent to the "overwhelmingly loud" address, as a False

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
