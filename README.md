# compi
Audio offset and comparison tool

**compi** is an application which takes a stereo audio input and checks whether the left-leg and right-leg are of the same audio. It can handle delays between the legs of upto any time the user wants (depending on the RAM of the system).

## How it works

* Audio is captured and deinterleved in to two separate buffers for a set period of time.
* Cross correlation is performed on the two buffers
* The start of the buffers are adjusted to match up in the time domain
* A perceptual hash is done of both buffers (using pHash) and the output of this is compared.
* This gives a confidence figure for how alike the two bits of audio are.
* If they are not the same then the buffers are expanded (in case the audio is the same just offset more than expected).
* The software provides an SNMP interface (using SNMP++ and Agent++) to retrieve the results of the offset and confidence calculations. 

## Prerequisites
### External libraries
**compi** depends on the following external libraries
* [portaudio](http://www.portaudio.com/download.html)
* [snmp++](https://agentpp.com/download/snmp++-3.4.2.tar.gz)
* [agent++](https://agentpp.com/download/agent++-4.3.1.tar.gz)

### Other source code included in the project
* [pHash](https://www.phash.org/) - source code for the audio perceptual hashing included in the project
* [kiss](https://github.com/mborgerding/kissfft) - included as a Git submodule
* [pml_log](https://github.com/martim01/log) - included as a Git submodule

## Building compi
todo
