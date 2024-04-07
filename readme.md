Linux driver for SSD1335 OLED display (the one by SparkFun, with a Quic/i2c interface). Sure, there is a mainline one somewhere in the depths of of the kernel, but that wasn't written by me. In any case, you should use that one, not this.

So far this is what's inside:

- Option to register up to 10 OLED screens (assuming you have enough i2c outputs)
- Register a separate framebuffer for each screen
- Initiate the screen (turn on and blank successfully)
- Turn off the screen upon removal
- FB blanking (through ioctl)
- Access the GPU RAM through `/dev/fbX`, for read and write also - the display can be used through this interface already.

Memory mapping is something that is missing sorely.

The device tree in this repo is for RPi, but considering that it isn't particularly complex, it shouldn't be a huge issue to adapt it for other devices.
