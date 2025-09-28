# USB433 - Sniff and Transmit

USB-connected, STM32F103-based 433.92MHz module with asynchronous RX sniffing for OOK protocol

<hr>

*Github Repo: <a href="https://github.com/MountainEclipse/USB433-STM32F103-RX-TX" target="_blank">USB433</a> by <a href="https://github.com/MountainEclipse" target="_blank">MountainEclipse (TimeHack)</a>*

*Assembly Instructable: <a href="" target="_blank">USB433 - Sniff & Transmit 433 MHz</a>*

<hr>

This project allows you to passively sniff On-Off-Keyed (OOK) signals in the environment - up to 64 bits in length - and transmit OOK messages via a USB-connected radio module. Originally designed to operate 433.92 MHz wireless outlet switches, this module can also receive weather station data and mimic other 433 MHz codes.

Designed to operate on a custom circuit board, this project should equally work with the hobby-friendly BluePill STM32F103 board with jumper wires to OOK RX/TX modules. All design and firmware files are provided as is, for personal hobbyist use under Creative Commons 4.0 Attribution, Non-Commercial (CC BY-NC) international license.

The following is a list of everything you need to make this project:
<ul>
  <li>STM32F103 Microcontroller</li>
  <li>A 433.92MHz OOK Transmitter and Receiver (I used the STX882 and SRX882 module pair)</li>
  <li>(Optional) 3D printer for building the project enclosure</li>
</ul>

<hr>
For instructions on assembly, flashing, and testing, please visit the project Instructable <a href="" target="_blank">here</a>