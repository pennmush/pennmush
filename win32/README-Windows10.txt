PennMUSH (mostly) works on the new Windows 10 linux compatibility layer.

Instructions:

1. Install Bash On Ubuntu For Windows (https://msdn.microsoft.com/en-us/commandline/wsl/about)
2. Install dependencies and build tools: At the bash prompt, run:
    sudo apt-get install make gcc libpcre3-dev libssl-dev libevent-openssl-2.0 gperf
3. Get the source for Penn if you don't already have it: At the bash prompt, run:
    wget http://download.pennmush.org/Source/pennmush-1.8.6p0.tar.gz
   Or whatever version you need. Alternatively, apt-get install git and use that:
    git clone https://github.com/pennmush/pennmush
4. Now extract like normal:
    tar xzf pennmush-1.8.6p0.tar.gz; cd pennmush-1.8.6p0
5. Configure Penn:
    ./configure

   Update 7/17: socketpair() and unix domain sockets now appear functional and
   the info and ssl slaves should work. If they give you problems, run configure with
   --disable-ssl_slave and --disable-info_slave.
   
6. Compile like usual:
    make update; make install
7. Edit game/mush.cnf as needed.
7. Run like usual:
    cd game; ./restart


