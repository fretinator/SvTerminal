# SvTerminal

Soft-scrolling Network Terminal with some ANSI Escape codes implemented,such as colors and the clear screen command.

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
![SvTerminal_1_0_0](https://github.com/fretinator/SvTerminal/assets/2607402/cc692423-c918-4eab-844a-96976f6b6d50)

