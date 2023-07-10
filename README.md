# SvTerminal

Soft-scrolling Network Terminal with some ANSI Escape codes implemented,such as colors and the clear screen command. Currently developed for M5Stack Core with the Faces keyboard. Should be easy to modify for other keyboard and screen combinations.

Requires a secrets.h files with this values populated:

// secrets.h

#ifndef SECRETS_H

#define SECRETS_H

const String host = "192.168.1.233";
const int port = 8080;
const char* my_ssid = "Your Wifi SSID";
const char* my_password = "YourWifiPassword";

#endif


On the Linux side, create a simple script that runs this command (example included - socket_listener.sh):

socat -d -d -d tcp-l:8080,reuseaddr,fork exec:'sudo /bin/login',pty,setsid,setpgid,stderr,ctty

NOTE:
Once I connect to the terminal, I issue the following commands:

  stty rows 20

  stty columns 50

This make the terminal size match my M5Stack screen.

![SvTerminal_1_0_0](https://github.com/fretinator/SvTerminal/assets/2607402/cc692423-c918-4eab-844a-96976f6b6d50)


