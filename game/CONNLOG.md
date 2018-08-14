Enhanced Connection Logging
===========================

Overview
--------

When the `use_connlog` option is turned on, the MUSH will log
connection information not just to the normal connection log, but also
to a Sqlite database (Named with the `connlog_db` option). This
database is persistant and is not reset between mush shutdowns and
startups, unless the format changes at some point in the future in
such a way it can't be upgraded without data loss.

The intent is that this is more easily parsed and queried than the
unstructured text of the normal connection log, making it easier to
use with monitoring tools. Said tools might eventually be written and
made available.

The wizard-only softcode `connlog()` and `connrecord()` functions let
you access the database from within a game and serve as an example of
usage of the tables in the database. They're defined in
*src/connlog.c*.

Internals
---------

There are four tables in the database:

* *checkpoint*, which only holds a single row - the time, which the
  mush updates every few minutes. This time is used when recovering
  from a crash or other unexpected shutdown. Connections that were
  established when the database was last updated are updated to
  disconnect at this timestamp.
* *timestamps*, which holds the connection and disconnection times of
  a connection. This is a virtual table using the Sqlite3 RTree
  extension, which allows for efficent queries of time intervals.
* *addrs* which holds unique IP addresses and the hostnames they
  resolve to.
* *connections*, which holds all other information about a
  connection - where it came from, who was logged in, and why they
  disconnected. Its **id** column pairs up with *timestamps.id*, and
  **addrid** with *addrs.id*. Most queries will join the tables on
  these keys, or just use...
* *connlog*, a view that joins all related connections, timestamps and
  addrs together into one comprehensive record.

plus assorted indexes to speed up queries. The *application_id* of the
database is `0x42010FF2`.

Examples
--------

To generate a table of the most common IP address each player
connects from:

    WITH addrcounts(name, ipaddr, c) AS
      (SELECT name, ipaddr, count(addrid) FROM
          connections AS conn JOIN addrs ON conn.addrid = addrs.id
         WHERE name NOT NULL
         GROUP BY conn.addrid)
    SELECT name, ipaddr, max(c) AS count FROM addrcounts
      GROUP BY name ORDER BY name COLLATE NOCASE;

To show all information about connections made in the last 24 hours:

    SELECT * from connlog WHERE conn >= strftime('%s', 'now', '-1 day');

To count the number of times a particular player has connected:

    SELECT count(*) FROM connections WHERE dbref=1234;
    
To show connections that never logged in:

    SELECT * from connlog WHERE dbref = -1;

Caveats
-------

One current known issue is that if a player logs out and logs back
into a different account without quitting, only the identity of the
last logged in player object is saved.
