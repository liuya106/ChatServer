# ChatServer
A TCP chat server that allow users to send messages and emojis

type in shell:

./chat_server to construct a chat server

./chat_client to connect as a client

The port might be occupied once we construct a server, you can add 1 on port number in the makefile to use another port. Remember to type in shell:

make clean

make

to refresh our c file everytime port was changed

The first-joined client is set to be the admin; admin types ".k username" to kick someone

Any user can send messages and emojis to each other; types ".e image.jpg" to send an emote in the directory "emotes"
