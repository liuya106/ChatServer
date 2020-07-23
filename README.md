# ChatServer
A TCP chat server that allow users to send messages and emojis

The first-joined client is set to be the admin; admin types ".k username" to kick someone

Any user can send messages and emojis to each other; types ".e image.jpg" to send an emote in the directory "emotes"

---------------------------------------------------------------------------------

type in shell:

./chat_server (construct a chat server)

in another shell:

./chat_client (connect as a client)

------------------------------------------------------------------------------------

The port might be occupied once we construct a server, you can add 1 on port number in the makefile to use another port. Remember to type in shell:

make clean

make

to refresh our c file everytime port was changed

-------------------------------------------------------------------------------------
Possible Problem:

If you are using WSL, then you might encounter:

mkfifo error: Operation not permitted

Open: no such file

This is because old WSL configuration does not permit creating named pipe on your machine, you can mount DrvFs with "metadata" flag, all you need to do is to type on terminal:

sudo umount /mnt/c

sudo mount -t drvfs C: /mnt/c -o metadata

Then you should be fine running this program, more details regarding to this problem can be found at: https://devblogs.microsoft.com/commandline/chmod-chown-wsl-improvements/