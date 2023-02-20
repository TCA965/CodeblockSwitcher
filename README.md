# CodeblockSwitcher
t4forum Thread:
https://www.t4forum.de/forum/index.php?thread/286463-datens%C3%A4tze-des-tdi-steuerger%C3%A4ts-ohne-vcds-wechseln-codeblock-switcher/&amp;pageNo=1

KW1281 Code based on:
http://grauonline.de/wordpress/?p=74

The CodeblockSwitcher is a device to recode the VAG TDI ECUs via K-Line (kw1281) (for example: MSA15, EDC15 in Golf mk4, T4, etc).



The ECUs are using different Codeblocks for different gearboxes:

00001: Automatic Gearbox

00002: Manual 2WD-Gearbox

00003: Manual 4WD-Gearbox (syncro / 4motion / quattro)


It is however possible to use this function for changing the power level of the engine.


So:

00001: Stock power output

00002: Street power (chiptuning)

00003: offroad / racing power




Recoding of the ECU is usually a job for VCDS / VAG-Com.
But this needs an computer....



So, i recreated the recode function on an µC (Atmega328PB).

It is small and can use any given Input method and display the current setting via leds or the cluster (if FIS is buitl in - 3LB) (three 12V tolerant Switch-Inputs, three 12V Outputs for LED oder relay and an 3LB-Interface for the cluster).

