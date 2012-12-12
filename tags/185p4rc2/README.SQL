
                            Use SQL with PennMUSH
                             Revised: 24 May 2004


As of version 1.7.7p32, PennMUSH includes functions and commands that
can access SQL databases. Currently, the following databases are
supported:

  * MySQL
  * PostgresQL (from 1.8.3p2)
  * Sqlite v3  (from 1.8.3p2)

This document explains how to use (or avoid) SQL with PennMUSH, and
covers the following issues:

   I. How SQL can be used
   II. Compiling with or without SQL
   III. Mush configuration overview
   IV. SQL setup tips

I. How SQL can be used
 
Penn doesn't use SQL for any storage internally. It just allows access
to a database via softcode functions (See 'help @sql' and 'help sql
functions' from in a game.) People have used it to hold information
that can be accessed from a game and from other programs -- for
example, a mail or bulletin board system with an in-game and a web
interface. Another common use is to hold data that can be queried via
SQL more convienently than in softcode. 

A word of caution: The sql functions are synchronous: Nothing else can
happen in the game until a query finishes. Complex queries over large
tables, or queries to a SQL server on a different host computer can
lag the mush for everyone!

II. Compiling with/without SQL

In general, configure attempts to detect all supported sql client
libraries on the host, and will link with all of them, permitting you
to select which platform you want at runtime. You can selectively
prevent linking with client libraries that are present on your system,
by passing configure a --without-FOO option. --disable-sql turns off
all checks for SQL servers.

For example, if your host has MySQL and Sqlite and you only want to
use the latter, you can invoke configure like so:

  % ./configure --without-mysql --with-sqlite3

II.a. MySQL

  The configure script distributed with PennMUSH automatically detects
  the MySQL client library via the presence of the mysql_config
  program, which will tell configure where needed headers and
  libraries are.

  If you want to avoid linking these libraries on systems where they
  are present, pass the --without-mysql switch to configure.

  If you installed Mysql from a binary package (e.g. rpm or apt), you
  should be sure that your system also has the development package
  (usuallyl mysql-dev or mysql-devel).

  If you think you have mysql libraries and header files but configure
  isn't finding them, they may be in an unusual location on your
  system, with mysql_config not in your default path.  Find its
  location ('where mysql_config' if the program works for you, 'find /
  -name mysql_config' otherwise, and call configure with
  --with-mysql=/path/to/mysql_config

II.b. PostgresQL

  The configure script distributed with PennMUSH automatically detects
  the PostgresQL client library via the pg_conf program, which will
  tell configure where needed headers and libraries are.

  If you want to avoid linking these libraries on systems where they
  are present, pass the --without-postgresql switch to configure.

  If you installed PostgresQL from a binary package (e.g. rpm or apt),
  you should be sure that your system also has the development package
  (usually postgresql-dev, or libpq or similar)

  If you think you have postgresql libraries and header files but
  configure isn't finding them, they may be in an unusual location on
  your system, with pg_config not in your default path.  Find its
  location ('where pg_config' if the program works for you, 'find /
  -name pg_config' otherwise, and call configure with
  --with-postgresql=/path/to/pg_config

II.c Sqlite

  The configure script distrubted with PennMUSH looks for sqlite3
  headers and libraries in /usr, /usr/local and /opt. If it's
  somewhere else on your system, call configure with
  --with-sqlite3=/path/to/sqlite3
 
  If you want to avoid linking with sqlite on systems where it is
  present, pass the --without-sqlite3 switch to configure.

  The path is the directory that contains the hdrs/ and lib/
  directories that hold the sqlite3 headers and library respectively.

III. Mush configuration overview

  mush.cnf includes these directives that configure the SQL support:

  sql_platform provides the name of the SQL database server software
  that will be used for connections. It current takes one of four
  values: "disabled" (no SQL), "mysql", "postgresql", or "sqlite3".
  If not specified, it defaults to disabled.

  sql_host gives the name of the host running the SQL server.  It
  defaults to 127.0.0.1, which makes a TCP connection to the local
  host. For MySQL, the keyword "localhost" instead makes a domain
  socket (Unix) or named pipe (Windows) connection. For PostgreSQL on
  Unix, if a path is specified (e.g. /var/run/postgresql), PennMUSH
  will use the domain socket connection found in that directory. You
  can also specify an alternate port by setting sql_host to
  hostname:port (e.g. 127.0.0.1:5444).

  sql_database gives the name of the database that contains the MUSH's
  tables. This must be specified and there is no default.
 
  sql_username provides a username to connect to the SQL server
  with. If no specified, a null username will be used, which many SQL
  servers treat as "the user running this (pennmush) process".

  sql_password provides the password for the user. It defaults to no
  password.
 
  For sqlite3, which uses a local file instead of connected to a
  database server, sql_database gives the name of the database file,
  sql_host must be localhost, and the username and password are
  currently ignored.

IV. SQL setup tips

  You will have to set up the appropriate database on the SQL server,
  and username permitted to perform operations in that database, and a
  password for that username. This is a platform-specific process.

  If you're not sure how to configure a SQL server, but want to use
  SQL from softcode, using the Sqlite3 library is the easiest route, 
  as it doesn't require a seperate server to be up and running. 

  A. MySQL platform

    Easiest way is:

    % mysql_setpermission --user root 			REQUIRED
          --host <mysql host> --port <mysql port>	OPTIONAL, OR:
          --socket <unix domain socket>			OPTIONAL

    ######################################################################
    ## Welcome to the permission setter 1.2 for MySQL.
    ## made by Luuk de Boer
    ######################################################################
    What would you like to do:
      1. Set password for a user.
      2. Add a database + user privilege for that database.
         - user can do all except all admin functions
      3. Add user privilege for an existing database.
         - user can do all except all admin functions
      4. Add user privilege for an existing database.
         - user can do all except all admin functions + no create/drop
      5. Add user privilege for an existing database.
         - user can do only selects (no update/delete/insert etc.)
      0. exit this program
   
    Make your choice [1,2,3,4,5,0]: 2                    <==========

    Which database would you like to add: mush
    The new database mush will be created
    
    What username is to be created: mush                 <==========
    Username = mush
    Would you like to set a password for  [y/n]: y       <==========
    What password do you want to specify for :           <==========
    Type the password again:                             <==========
    We now need to know from what host(s) the user will connect.
    Keep in mind that % means 'from any host' ...
    The host please: localhost                           <==========
    Would you like to add another host [yes/no]: no      <==========
    Okay we keep it with this ...
    The following host(s) will be used: localhost.
    ######################################################################
    
    That was it ... here is an overview of what you gave to me:
    The database name       : mush
    The username            : mush
    The host(s)             : localhost
    ######################################################################
    
    Are you pretty sure you would like to implement this [yes/no]: yes

  B. PostgreSQL

  % sudo -u postgres psql
  postgres=# CREATE USER mush WITH PASSWORD 'mush';
  CREATE ROLE
  postgres=# CREATE DATABASE mush OWNER musher;
  CREATE DATABASE
  postgres=# \q

  If you are using domain sockets on Unix and don't need to use password
  authentication, simply omit "WITH PASSWORD 'mush'".

  C. Sqlite3:
    
    As the same user account that runs the mush:
       Edit mush.cnf so that sql_database is set to data/mush.db and
       sql_platform is set to sqlite3.
       % cd pennmush/game/data
       % sqlite3 mush.db
         > create table ....

