# CodeblockSwitcher
t4forum Thread:
https://www.t4forum.de/forum/index.php?thread/286463-datens%C3%A4tze-des-tdi-steuerger%C3%A4ts-ohne-vcds-wechseln-codeblock-switcher/&amp;pageNo=1

KW1281 Code based on:
http://grauonline.de/wordpress/?p=74

The CodeblockSwitcher is a device to recode the VAG TDI ECUs via K-Line (kw1281) (for example: MSA15, EDC15 in Golf mk4, T4, etc).

<br>

The ECUs are using different Codeblocks for different gearboxes:

00001: Automatic Gearbox

00002: Manual 2WD-Gearbox

00003: Manual 4WD-Gearbox (syncro / 4motion / quattro)

<br>

It is however possible to use this function for changing the power level of the engine.


So:

00001: Stock power output

00002: Street power (chiptuning)

00003: offroad / racing power

<br>
<br>


Recoding of the ECU is usually a job for VCDS / VAG-Com.
But this needs an computer....

<br>

So, i recreated the recode function on an µC (Atmega328PB).

It is small and can use any given Input method and display the current setting via leds or the cluster (if FIS is buitl in - 3LB) (three 12V tolerant Switch-Inputs, three 12V Outputs for LED oder relay and an 3LB-Interface for the cluster).



<br>
<br>

The "usual" K-Line communication starts like this:

- 5 baud init with address of the ECU (0x01)
- Answer of the ECU with device data 1/4
- ACK to ECU
- Answer of the ECU with device data 2/4
- ACK to ECU
- Reply from ECU with device data 3/4
- ACK to ECU
- Reply from ECU with unit data 4/4
- ACK to ECU

Dump of an MSA15 (with Complements):
00 00 55 01 8A 75 - INIT

1B E4 01 FE F6 09 30 CF 37 C8 34 CB 39 C6 30 CF 36 C9 30 CF 32 CD 31 CE 41 BE 20 DF 20 DF 32 CD 2C D3 35 CA 6C 93 20 DF 52 AD 35 CA 20 DF 45 BA 44 BB 43 BC 20 DF 03 - "074906021A 2,5l R5 EDC "
(first ASCII data block)

03 FC 02 FD 09 F6 03 - ACK from PC


07 F8 03 FC F6 09 20 DF 47 B8 30 CF 30 CF 03 - " G00"
(second ASCII data block)

03 FC 04 FB 09 F6 03 - ACK from PC


0B F4 05 FA F6 09 53 AC 47 B8 20 DF 20 DF 31 CE 39 C6 30 CF 39 C6 03 - "SG 1909"
(third ASCII data block)

03 FC 06 F9 09 F6 03 - ACK from PC


08 F7 07 F8 F6 09 00 FF 00 FF 04 FB 03 FC C5 3A 03 
(fourth ASCII data block)

03 FC 08 F7 09 F6 03 - ACK from PC

The fourth block is exciting: although ASCII data is displayed (0xF6 as Block Title), no ASCII data is received. If you remove the complements, the following remains:

0x00 0x00 0x04 0x03 0xC5.
This can be translated into:
MSB_Coding: 0x00
LSB_Coding: 0x04
--> Coding 0004 (HEX)
This must be divided by two, i.e.: Coding 00002. (If the ECU is coded to 00003, 0006 (HEX) appears).

MSB_WSC: 03
LSB_WSC: C5
--> Workshop code: 965

So: The fourth data block transmits the coding and the workshop code.

Now to change the coding:

Only the title block is important here:
0x10

This results in the following "Change coding to 00001 and WSC to 00965" block:
Block Length: 07 
Block Counter: 9E 
Block title: 10 
Block Data: 00 02 03 C5
Block End: 03

Block Data looks familiar:
00 02 03 C5 -->

0002: Target coding * 2 -> 00001
03C5: WSC -> 00965

After sending the coding command, no ACK block follows, but the ECU answers again with the device data.
So three times ASCII + once "pseudo-ASCII". We evaluate the fourth block again:

Block Length:08 
Block Counter:A5 
Block Title: F6 
Block Data: 00 00 02 03 C5
Block End: 03

So:
0x00 0x02 0x03 0xC5 --> Coding 00001 (0x0002 / 2) - 965 (HEX 03C5)

...success!

