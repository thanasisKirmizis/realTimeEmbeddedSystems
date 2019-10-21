# EE AUTh Assignment for Real Time & Embedded Systems 2018-2019

## Description

This application is intended to work as a peer-to-peer messaging exchange application between multiple Raspberry Pis Zero W.
The communication between the nodes is done through an ad-hoc WiFi that the devices setup. By setting the right IP (according to the src code) and WiFi credentials to each device, the connection is being done without the need of any other effort.

Statistics of the duration of each session between the devices is saved to .txt files so that further analysis can be made. 

## Usage

To be able to run properly in the RPis, the C source code can be cross-compiled with these tools https://github.com/abhiTronix/raspberry-pi-cross-compilers . The produced executable file can be transferred to the RPi through SSH (either from Linux or Windows with Putty).
