# medley

Pick a folder with wav audio files and let this C program merge all tracks into a medley.

## Description

A medley gives you the best of a couple of songs in one peace. Wouldn't it be nice to have a medley for every album you wanted to listen to? ... to know what it sounds like? ... to remember it by?

## How to install

Download the distribution from here or type

```wget asdfasdf```

Unzip the distribution zip file:

```unzip distro.zip```

Compile from source using make or your own favorite compiler:

```make asdfasdf```

Run the program

```./medley dir-with-songs```

## Usage

To perform audio edits and output your medley, the program takes the following parameters:

- the **location** of the folder containing the audio files
- name of the outputted **file**, not including the file type .wav
- **in-marker** in seconds from the beginning of the track
- **duration** in seconds from the beginning of the track
- the length of the **crossfade** between each of the tracks

### Parameters and defaults

Parameters are optional. If not specified via the respective flag, default values are used:

| Parameter | Flag | Default | Description |
|-|-|-|-|
| location | -r | audio | **r**ead source directory |
|file|-w|medley.wav|**w**rite to output file|
|in|-i|10|**i**n-marker in seconds|
|dur|-o|20|**d**uration in seconds|
|x-fade|-x|2|**x**fade duration in seconds|

### Examples

```./medley /beatles -f beatles.wav -i 40 -d 10 -x 1```
Take all wav files in the folder beatles and create the medley beatles.wav with 10 Seconds (0:40 to 0:50) for each song using 1 second to blend between them.

```./medley /slipknot -i 0 -d 10 -x 0```

Take any .wav file in the slipknot folder and generate a medley.wav made up of the first 10 seconds of every song.

```./medley /coldplay -f elevator.wav -i 40 -d 20 -x 10```

Produce some everblending elevator music ;P

### Remarks

The length specified for the crossfade will also be used for the fade in (first track) and the fade out (last track).

Explicitly setting the crossfade duration to 0 will result in a hard and audible cut. A minimum length of 0.1 is recommended (100ms).

The crossfade cannot be longer that the actual snippet or song, so a 5 second crossfade on in-marker 10 and out-marker 12 will result in an error. So does an in-marker of 0 and any crossfade length other than 0.

### Limitations

Only uncompressed wave files are supported (.wav, .wave, .bwf). Other files will be ignored.

All files must have the same number of channel (mono, stereo, surround, any). So there is no downmix or summing involved.

All files must have the same sample frequency (e.g. 44.1kHz, 48kHz), sorry no sample rate conversion.

All files must show the same bit depth (e.g. 16, 24) and use fixed point notation. No interpolation or dithering going on.

Encountering a deviating file (channel, sample rate, bit depth) will generate an error. Why not temporarily move the file some place else and try again :)

---
## Under the hood

