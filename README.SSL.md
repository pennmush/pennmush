% Use SSL with PennMUSH
%
% Revised: 04 Jan 2018

Introduction
============

As of version 1.7.7p17, PennMUSH supports SSL connections when linked
with the OpenSSL library (<https://www.openssl.org>). As of version 1.8.4p9, 
OpenSSL is a requirement. OpenBSD's LibreSSL also works.

The following features are supported:

* Encrypted sessions using TLSv1 and TLSv2 protocols
  with ephemeral Diffie-Hellman keying.
* Authentication of the server via certificates
* Authentication of the clients via certificates
* Use of digest routines in the crytpo library for encrypting
  passwords and the digest() function.

An SSL overview
===============

When an SSL client connects to an SSL server, it performs a
"handshake" that looks something like this:

    Client says hello, offers a menu of cipher options
    Server says hello, selects a cipher.
    Server presents its certificate, requests a client certificate
    Client presents a certificate (or not)
    Client and server exchange cryptographic session keys

The server is identified to the client by a certificate, an encoded
text that gives the server's name and other attributes and is signed
by a certifying authority (CA), like Verisign. The client checks that
the signature is by a CA that it trusts, and may perform other
validation on the certificate (e.g., checking that the hostname in the
certificate matches the hostname it's trying to connect to).

If the client chooses to present a certificate (or is required to by
the server), the server will likewise attempt to validate it against
its list of trusted CAs, and may perform other verification.

Once session keys have been exchanged, the client and server can
communicate secure from eavesdropping.

Compiling with OpenSSL
======================

What to install
---------------

The configure script distributed with PennMUSH automatically detects
the OpenSSL libraries (libssl and libcrypto) and sets up the proper
compiler flags to use them.

If you install it through your operating system's package management
system, you need shared libraries and development headers. (Packages
with names like openssl, libssl, and -dev or -shlibs suffixes are
common. Exact names vary from OS to OS. You want at least verison
0.9.7.) If OpenSSL gets installed in a place that isn't checked by
default, you can invoke configure with `./configure --with-ssl=/path/to`
(The path must be the root directory of where OpenSSL's include/ and
lib/ directories are.)

OpenSSL can also be compiled on Windows, and you could add its
libraries to the PennMUSH project file and link it in that way.  It's
easier to use something like MSYS2 though - see the README files in
win32/.

Persistent SSL connections
--------------------------

Normally, encrypted connections will be booted when a running game
restarts, because the internal OpenSSL state cannot be saved to disc
and restored later. To allow persistant ssl connections via a proxy
process, pass --enable-ssl_slave to configure when you run it. This
does not work on Windows.

It requires the libevent 2.X library and headers to be installed. Get
it through your OS's package system or at <http://libevent.org/>. If
libevent is detected, the ssl slave is automatically enabled.
 
To use the ssl_slave, a full `@shutdown` and restart of the mush has
to take place after the mush source has been configured to enable it,
and compiled. Then players can connect via SSL capable clients exactly
as they did before, with the only change being that they won't get
booted on a `@shutdown/reboot`. It's transparent to the player and the
game.

MUSH configuration overview
===========================

mush.cnf includes a number of directives that control SSL configuration:

`ssl_port`

:    selects the port number for the MUSH to listen for SSL
     connections. Any port number other than the MUSH's ordinary
     listening port can be chosen (subject, of course, to other system
     restrictions on choosing port numbers).

     If left blank, SSL connections will not be enabled. However, other
     parts of the mush, such as the `digest()` softcode function, and
     password encryption, will still use OpenSSL library routines.

`ssl_ip_addr`

:    Controls the IP address to listen on. When in doubt, leave blank.

`ssl_private_key_file`

:    Specifies the name of the file (relative to the
     game/ directory if it's not an absolute path) that contains the MUSH
     server's certificate and private key. See section IV below.

`ssl_ca_file`

:    The name of the file that contains
     certificates of trusted certificate authorities. OpenSSL distributes
     a file containing the best known CAs that is suitable for use here.
     If you comment this out, client certificate checking will not be
     performed.
     
     Defaults to /etc/ssl/certs/ca-certifcates.crt

`ssl_ca_dir`

:    A directory containing multiple certificate files.

     Defaults to /etc/ssl/certs

`ssl_require_client_cert`

:    A boolean option that controls whether
     the MUSH server will require clients to present valid (that is,
     signed by a CA for which ssl_ca_file holds a certificate)
     certificates in order to connect. As no mud clients currently do
     this, you probably want it off. See section V below.

`socket_file`

:    The path to a file to use as a unix domain socket
     used for talking to the optional SSL connection proxy.

Installing a server certificate
===============================

SSL support requires that the MUSH present a server certificate
(except as discussed below).  You must create a file containing the
certificate and the associated private key (stripped of any passphrase
protection) and point the `ssl_private_key_file` directive at this
file. This file should only be readable by the MUSH account!

How do you get such a certificate and private key? Here are the steps
  you can use with openssl's command-line tool:

1. Generate a certificate signing request (mymush.csr) and a private
   key (temp.key). You will be asked to answer several questions.
   Be sure the Common Name you request is your MUSH's hostname:

        $ openssl req -new -out mymush.csr -keyout temp.key -passin pass:foobar

2. Strip the passphrase off of your private key, leaving you with an
   unpassworded mymush.key file:
 
        $ openssl rsa -in temp.key -out mymush.key -passin pass:foobar
        $ rm temp.key

3. Send the certificate signing request to a certifying authority to
   have it signed. If the CA needs the private key, send the
   passphrased one. The CA will send you back a certificate which you
   save to a file (mymush.crt)

4. Concatenate the certificate with the unpassworded private key and
   use this as the `ssl_private_key_file`:

        $ cat mymush.key >> mymush.crt

Commercial CAs like Verisign sign certificates for a yearly or
two-yearly fee that is probably too steep for most MUSH Gods. Instead
of using a commercial CA, you can generate a self-signed certificate
by changing step 1 above to:

    $ openssl req -new -x509 -days 3650 -out mymush.crt -keyout temp.key -passin pass:foobar

A self-signed certificate is free, but clients that attempt to
validate certificates will fail to validate a self-signed certificate
unless the user manually installs the certificate in their client and
configures it to be trusted. How to do that is beyond the scope of
this document, and highly client-dependent.

Another option is to skip the use of a certificate altogether.  If you
don't provide an `ssl_private_key_file`, the server will only accept
connections from clients that are willing to use the anonymous
Diffie-Hellman cipher; it is unknown which clients are configured to
offer this. This provides clients with no security that they are
actually connecting to your server, and exposes them to a
man-in-the-middle attack, but requires no work on your part at all.

Hosting providers or other parties may one day provide CA service to
PennMUSHes for free. When they do, you'll have to install those CAs'
certificates in your client as trusted in order to have the server's
certificate validate, but if a few CAs certify many MUSHes, this is
efficient.

Using client certificates for authentication
--------------------------------------------

If you provide PennMUSH with a file containing the certificates of
trusted CAs (using the `ssl_ca_file` directive in mush.cnf), it will, by
default, request that clients present certificates when they connect.
Clients that do not present certificates will still be allowed to
connect (unless `ssl_require_client_cert` is enabled).
 
Clients that do present certificates must present certificates signed
by a trusted CA, or they will be disconnected. Both valid and invalid
certificates are logged (to connect.log and netmush.log,
respectively).

If you were really serious about this, you probably would issue your
own certs and not allow Verisign, etc. certs. You'd probably want to
have the server validate extra attributes on each client cert, which
should probably include the player's dbref and creation time. This is
left as an exercise for the reader for now.
