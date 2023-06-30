

# Development of a Raspberry Pi-based Readout for Intensified Detectors


## Overview



## Table of Contents

- [Project Description](#project-description)
- [Installation](#installation)
- [Usage](#usage)
- [License](#license)

## Project Description

This code perfoms photon counting and centroiding on the image of the phosphor screen of an intensified detector. The code takes the image stream from the piped input and performs the necessary calculations. The code also transmits the centroid data to the UART port
## Installation 

Library dependencies: WiringPi, OpenCV

```bash
sudo apt-get install git-core
git clone git://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
```
```bash
sudo apt-get install libopencv-dev
```
Hardware required.

1. Raspberry Pi Zero 2w
2. Raspberry Pi Camera Module V2
 
## Usage

To compile
    
```bash
$ make all
```
To run the code 
    
```bash
$ make run
```
Output will throught the serial port.
Pin number 5 in WiringPi is used select mode of operation. 

0 - 3X3 Mode

1 - 5X5 Mode



## License

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

```plaintext
Copyright 2023

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```



