# (A)rduino (S)im (D)ata (A)cquisition (S)ystem

![alt asdas_station](https://raw.githubusercontent.com/laresistenciadelbit/asdas_station/main/readme-images/last_boats.jpg)

[FULL DOCUMENTATION (SPANISH)](https://github.com/laresistenciadelbit/varios/raw/master/ASDAS_PUBLIC.PDF)

[FULL DOCUMENTATION LITE (SPANISH)(NO CODE DIAGRAMS)](https://github.com/laresistenciadelbit/varios/raw/master/ASDAS_PUBLIC_LITE.PDF)

## SPANISH

Arduino Sim Data Acquisition System es un proyecto que cubre toda la parte de
adquisición de datos de sensores desde el circuito físico, a la transmisión de los datos,su almacenaje, y su visualización en tiempo real desde un navegador Web.

Un dispositivo estación hecho con Arduino envía peticiones JSON HTTP POST al servidor.

Este repositorio de Github almacena la parte del dispositivo estación.

La parte del servidor la puedes encontrar en:
[https://github.com/laresistenciadelbit/asdas](https://github.com/laresistenciadelbit/asdas)

#### ESTACIÓN ASDAS

![alt asdas_station_circuit](https://raw.githubusercontent.com/laresistenciadelbit/asdas_station/main/readme-images/END_marks.jpg)

La estación cuenta con un módulo SIM para mandar los datos por la red móvil, un microcontrolador (Arduino Pro Micro versión 3.3V), un lector de tarjetas Micro SD para almacenar los datos que no se hubiesen podido enviar por falta de señal, y un sistema de autonomía con batería, panel solar y un conversor de voltaje que distribuye la energía y carga la batería. También se le añadirán tantos sensores como se desee (hasta el límite de entradas del microcontrolador)


#### CARPETAS DEL PROYECTO

- arduino_sketch

>Aquí encontrarás el sketch para compilarlo en arduino. 

>He tomado la librería oficial de sim808 y la he modificado para hacerla compatible con más módulos y he añadido alguna función.
Es necesario usarla para que funcione el sketch:

[https://github.com/laresistenciadelbit/sim8xx](https://github.com/laresistenciadelbit/sim8xx)

>###### Se instala como el resto de librerías: Se pega su contenido en la carpeta libraries de la carpeta Arduino (o bien la que aparece en mis documentos o bien en la de instalación del programa)

.
- kicad_project

>Aquí encontrarás los esquemas del prototipo de pcb y circuito en formato kicad

.

- blender

>Aquí encontrarás los modelos 3d que forman la estructura para el prototipo

.

![alt asdas_station_circuit_timelapse](https://raw.githubusercontent.com/laresistenciadelbit/asdas_station/main/readme-images/TIMELAPSE.gif)


___
.
## ENGLISH

ASDAS is a project which covers all the data acquisition part of sensors starting from the physical circuit followed by the data transmission, its storage and visualization in real time from a Web Browser.

A station device made with Arduino send HTTP POST JSON messages to the server.

This Github project is the station part.

The server Github can be accessed from:

[https://github.com/laresistenciadelbit/asdas](https://github.com/laresistenciadelbit/asdas)


#### ASDAS STATION

![alt asdas_station_circuit](https://raw.githubusercontent.com/laresistenciadelbit/asdas_station/main/readme-images/END_marks.jpg)

The station counts with a Sim module for sending data to the server throught the mobile network, a microcontroller (Arduino Pro Micro 3.3V), a micro SD reader for storing the data that wouldn't be delivered to the server by signal fail, and an autonomous energy system with battery, solar panel and voltage conversor which distributes energy and charges the battery. Also ther will be added so many sensors as is wished (until the microcontroller I/O ports limit)

#### PROJECT FOLDERS

- arduino_sketch

>Here you can get the sketch for compiling in Arduino

> I've took the official sim808 library and I've modified it for making compatible with more Sim modules and I've also added some function.
The library must be used with this sketch to work:
[https://github.com/laresistenciadelbit/sim8xx](https://github.com/laresistenciadelbit/sim8xx)

>###### The install process for the library is the same like other libraries: Just paste its content in the "libraries" folder from the install directory or "my documents/Arduino" directory.

.
- kicad_project

>Here you can get the pcb and circuit schemas in KiCad format.

.

- blender

>Here you can get the 3d models of the created structure for the prototype

.

![alt asdas_station_circuit_timelapse](https://raw.githubusercontent.com/laresistenciadelbit/asdas_station/main/readme-images/TIMELAPSE.gif)
