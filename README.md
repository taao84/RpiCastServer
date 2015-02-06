# RpiCastServer
A server implementing the DIAL protocol intended to run in a Raspberry Pi to give a similar functionality than a Chromecast device.

NOTE: At this moment the content of this project is just a copy of the demo-example provided by http://www.dial-multiscreen.org/example-code
Only the server code is covered here.

This project is meant to be an educational project. The topics to learn with this project cover: 
- C programming
- Building of C code using maen and the nar-maven-plugin
- Networking
- Implementation of protocols (DIAL in this case)

Optionals:
- Crosscompilation to ARM architectures (Rpi)

NOTES: The default configuration of the project will create an i386 executable, then you will not be able to use it in a Raspberry Pi. However at the root of the project you can find a folder named "optional_build" with the makefile that you can use to do compile the project using a crosscompiler for ARM. I will be uploading the steps to do so soon, for the ones that do not know how to do it. Optionally you can just follow the instructions given by the DIAL project in the given URL (http://www.dial-multiscreen.org/example-code).

LIMITATIONS
The Raspberry Pi B+ do not have the processing power or memory to run a Chrome browser displaying video. I tried this but it goes extremelly slow. However I will be doing some tests on the new Raspberry Pi 2 (pretty lady with bigger boobs and bigger ass), hopefully we will be able to get a good RpiCast going with this new one.

