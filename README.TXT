The Kijani project
Origional Auther:
Marc d'Hotman de villiers

**** History ****
This project was started as the auther was interested in the antclass combat compititions at the South African Robot Leuage(2024). Robots were expensive as you needed so many parts at it seems so much easier to make a robot with an ESP32. The origional was controlled with bluetooth via a MIT Inventor android app.  
In 2025 I was given the oprtunity to help with the Rover moot that was set for Decemeber that year(bummer is did not happen). This made the change to have the project work on all phones and try to make it easier for mass production. The V2 board was made 5mm shorter and removed one of the regulators making some limits to the project.

**** PCB ****
The pcb has the ESP32 at its core withe the ch340 built in to facilitate programming and debugging along with the we have 2 motor drivers 1 step up regulator that can be controlled by the ESP for 2 different voltages, a charger for a single cell lipo and access to 2 servos. This allows you to make a robot that operates in several different ways. The standard kit given at the moot has two motors and 1 servo as the flipper. However you cvan use one motro to drib a set of motors and the servo to stear or have a motor as a spinner weapon. There are many possibilities. If you want a pcb made please use the link for it on Easy eda or see the files in the hardware folder

**** Firmware ****
There are two parts to the software on board, the first is the c code used to run the ESP this can be programmed via the serial port using vscode as standard. The second is the html/javascript in the filesystem. You can also use the serial port to update this or via the build in system wifi Access point. This alloows you to make whatever interface you want for your robot.

**** Setup ****
Assuming you have programmed the default software you should be able to connect to the access point created on startup. From here you can set a password so you are the only one to have access and setup the rest opf your robot. 

**** Factory reset ****
If you have forgotten your password you can factory reset your board by shorting out the pgm pins while booting up. you should hear a differt tune coming from any motors connected.

**** Warnings ****
The voltage on the board can be made higher than what some servos can use, make sure before enabling the higher voltage

**** api ****
This projhect has been designed so you can modify everything one of them being the web interface for the controlls. Sending posts can allow you to control the bot.