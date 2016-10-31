# SSRCON
Stupid Simple RCON, a basic C++ terminal program to talk to an RCON server. This version has had the protocal tweaked to make it complient with what FortressCraft Evolved expects, note, the FCE server will not respond if the password is wrong, so the program will appear to hang currently.

Arguments:

-d 1-3 (debug mode)

-s address (server address)

-p port (server port)

-u user password (rcon user password)

Exmaple:

./SSRCON -d 1 -s 127.0.0.1 -p 27015 -u Password
