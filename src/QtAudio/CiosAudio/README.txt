CIOS Audio Core

Home : http://ciosaudio.sourceforge.net

In most cases, what you should do is to include the files you need.
We suggest you pick the modules and compile the source code with your
own project. The code in each Host APIs are separated except FFmpeg.
Just pick what you need and incorporate into your own program.

Platforms:

Windows.txt
MacOSX.txt
Linux.txt

These files contain a list of what you need.

Expertimental platforms:

Solaris.txt
FreeBSD.txt
iPhoneOS.txt
Android.txt

If you are using Qt, just include CiosAudio.pri in your PRO file.

If you are trying to compile CIOS Audio Core into library,
try "Compile" directory, look for the platform you need.

You should read the "Documents" for detail settings.

Linux settings

CAC_NAMESPACE
REMOVE_DEBUG_MESSAGE
CAUTILITIES
FFMPEGLIB
ENABLE_HOST_ALSA
ENABLE_HOST_OSS
ENABLE_HOST_JACK
ENABLE_HOST_ASIHPI
ENABLE_HOST_FFMPEG
ENABLE_HOST_OPENAL
ENABLE_HOST_PULSEAUDIO
ENABLE_HOST_VLC


Mac OS X settings

CAC_NAMESPACE
REMOVE_DEBUG_MESSAGE
CAUTILITIES
FFMPEGLIB
ENABLE_HOST_COREAUDIO
ENABLE_HOST_FFMPEG
ENABLE_HOST_OPENAL
ENABLE_HOST_PULSEAUDIO
ENABLE_HOST_VLC


Windows settings

CAC_NAMESPACE
REMOVE_DEBUG_MESSAGE
CAUTILITIES
FFMPEGLIB
ENABLE_HOST_DIRECTSOUND
ENABLE_HOST_WASAPI
ENABLE_HOST_WDMKS
ENABLE_HOST_WMME
ENABLE_HOST_FFMPEG
ENABLE_HOST_OPENAL
ENABLE_HOST_PULSEAUDIO
ENABLE_HOST_VLC
