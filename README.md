# NuMaker_NuWicam_BSP

## Firmware Build Steps

0. Required Linux host OS

    - Ubuntu 15.04 32Bit (Tested)
    - Ubuntu 16.04 32Bit
    - Ubuntu 16.04 64Bit

1. Clone

    sudo apt-get install git

    git clone https://github.com/OpenNuvoton/NuMaker_NuWicam_BSP.git

2. Build

    cd NuMaker_NuWicam_BSP
    
    scripts/build.sh

3. Check
    
    ls -al output/autowriter

## Firmware Programming

    Please refer Autowriter chatper in NuMaker_NuWicam_BSP/doc/Nuvoton NuMaker NuWicam User Guide.pdf document.
