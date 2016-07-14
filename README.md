# NuMaker_NuWicam_RDK

This repository is for Nuvotom NuMaker NuWicam Reference Design Kit. It includes board schematics, firmware, tools and documents resources for your reference.

## Firmware Build Steps

0. Required Linux host OS

    - Ubuntu 15.04 32Bit (Tested)
    - Ubuntu 16.04 32Bit (Tested)
    - Ubuntu 16.04 64Bit

1. Clone

    sudo apt-get install git

    git clone https://github.com/wosayttn/NuMaker_NuWicam_RDK.git

2. Build

    cd NuMaker_NuWicam_RDK
    
    scripts/build.sh

3. Check
    
    ls -al output/autowriter

## Firmware Programming

    Please refer Autowriter chatper in NuMaker_NuWicam_RDK/doc/Nuvoton NuMaker NuWicam User Guide.pdf document.
