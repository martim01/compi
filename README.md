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
You first need to make sure the correct build tools are installed
* cmake 
* pkg-config 
* autoconf
```
sudo apt install cmake pkg-config autoconf
```
Next download snmp++ and agent++ either from the website or from the command line
```
wget https://agentpp.com/download/snmp++-3.4.2.tar.gz
wget https://agentpp.com/download/agent++-4.2.0.tar.gz
```
Note the latest version of agent++ will not compile with SNMPv3 support disabled.

Extract the tar.gz files and build each library
```
tar -xf snmp++-3.4.2.tar.gz
cd snmp++-3.4.2.tar.gz
./configure --disable-snmpv3
make
sudo make install
```
```
tar -xf agent++-4.2.0.tar.gz
cd agent++-4.2.0.tar.gz
./configure
make
sudo make install
```

Install the portaudio library
```
sudo apt install portaudio19-dev
```


Enter the **compi** directory, create a **build** directory and run CMake
```
cd compi
mkdir build
cd build
cmake ..
make
sudo make install
```
This will 
* build **compi** 
* install it in **/usr/local/bin** 
* copy the **compi.ini** configuration file from **compi/config** to **/usr/local/etc**

To run
```
compi /usr/local/etc
```
