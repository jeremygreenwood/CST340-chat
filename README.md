CST340-chat
===========

collaborative chat server



FORMAT:

    As a general guideline, please use formatting similar to what is pre-existing for code consistency and readability.

    Please do not use camelCase, but rather use underscores_between_words for variable and function names.

    Use spaces instead of tabs, with 4-space indentation.  This will keep the code aligned well for everyone
    who views it.  In Eclipse IDE this option is available under:
    Project > Properties > C/C++ General > Formatter > Configure Workspace Settings... > Edit... > Indentation (tab):
        set "Tab policy" to "Spaces only"
        set "Indentation size" to "4"
        set "Tab size" to "4"
        
    Alternatively you may use project specific format settings for Eclipse IDE (if applicable).  This will setup spaces only
    as detailed above and setup consistent format settings for the project.

    To setup in Eclipse:

        Window > Preferences > C/C++ > Code Style > Formatter > Import... > select code_format_for_eclipse.xml
    	
    To format a chunk of code after setup:

        highlight code > right click > Source > Format
        (or highlight and use Ctrl + Shift + F hotkey)


CODING GUIDELINES:

    Please do not use "majic numbers" but rather #define any constants in your code.  Some exceptions apply to this,
    but please use your best judgement.
    

BUILD:

    run the following commands to build the chat server application:

    cd Debug
    make


RUN:

    Note: additional steps are necessary to allow connections past a firewall on the server.
    
    
    Example 1:
    
	    run the following command to run the chat server application with default port (3456):
	    
	    	./CST340-chat
	    
	    run the following command on the client to connect:
	    
	    	telnet <server_ip_address> 3456
    
    
    Example 2:
    
	    run the following command to run the chat server application with port 3555:
	    
	    	./CST340-chat 3555
	    
	    run the following command on the client to connect:
	    
	    	telnet <server_ip_address> 3555
