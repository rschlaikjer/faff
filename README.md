# faff

A tool for flashing bitstreams to FPGA boards, when paired with an appropriate
microcontroller firmware (seen
[here](https://github.com/rschlaikjer/hx4k-pmod/tree/master/programmer_firmware))

## Building

To build `faff`, you only need the core build tools for your platform, CMake,
and `libusb`.

    # Install dependencies
    sudo apt install -y build-essential cmake libusb-dev
    # Clone the repo
    git clone git@github.com:rschlaikjer/faff.git
    # Build as a normal CMake project
    mkdir faff/build
    cd faff/build
    cmake ../
    make -j$(nproc)
    # Optionally
    sudo cp faff /usr/local/bin

## Usage

    faff: Find and Flash FPGA
    Common usage: faff -f top,bin

    General options:
        -h|--help              This help message
        -f|--file  <binary>    The file that should be written to the target
        -e|--enumerate         Print a list of connected device series that
                               match the specified VID:PID
        --lma <address>        The load memory address to use for the file.
                               Defaults to 0x0000
        --no-verify            Disable reading back the programmed file to
                               verify that programming was successful.
    Target selection:
        --usb-vid <vid>        Set vendor ID of device to use
        --usb-pid <pid>        Set product ID of device to use
        --usb-serial <serial>  Select device with specific serial <serial>. If not
                               specified, will attempt to program the first device
                               found with a matching VID:PID

