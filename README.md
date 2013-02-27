CST340-chat
===========

collaborative chat server


TODO:

Convert all code to spaces instead of tabs.


FORMAT:

Please use formatting similar to what is pre-existing for code consistency and readability.

Use spaces instead of tabs, with 4-space indentation.  This will keep the code aligned well for everyone
who views it.  In Eclipse IDE this option is available under
Project > Properties > C/C++ General > Formatter > Configure Workspace Settings... > Edit... > Indentation (tab):
    set "Tab policy" to "Spaces only"
    set "Indentation size" to "4"
    set "Tab size" to "4"


BUILD:

run the following commands to build the chat server application:

cd Debug
make


RUN:

Note: additional steps are necessary to allow connections past a firewall.

Example 1 - run the following commands to run the chat server application with default port (3456):

./CST340-chat

Example 2 - run the following commands to run the chat server application with port 3555:

./CST340-chat 3555
