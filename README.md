Comfirm DMARC Report Server ("Comdarc")
======================================
*v 0.7*

Server for receiving **DMARC** reports, validate their integrity, and passing them to a web service as JSON.
Created by the good guys at [Comfirm](http://comfirm.se).


Features
--------

* Written in C with performance in mind.
* Acts as an SMTP server.
* Validates DKIM signatures for data integrity.
* Passes the reports in "real-time" to a web service as JSON data.
* Persistent queueing of web service requests. I.e. if the web service is down then the reports will be sent as soon as it gets up. Also means that it can survive a restart.

Install
-------

**First**, install the libraries liburl, libxml2, openSSL and zlib.

**Then install the extended firm-dkim:**
    
    $ cd firm-dkim-extended
    $ make
    # make install
    $ cd ..
    
**Now install the server:**

    $ ./configure
    $ make
    # make install

**Configure the server:**

Open */etc/comfirm-dmarc/comfirm-dmarc.conf* and edit it after your needs.

**Start the server by typing:**
    
    # comfirm-dmarc -c /etc/comfirm-dmarc/comfirm-dmarc.conf


If everything is setup, you could test it by running the included tests and examples.

When you are ready to go all you is to setup a web service and an MX pointer to your server.
(And ofcourse the DMARC record)

Dependencies
------------

* firm-dkim-extended *(included, used for signature validation)*
* libcurl *(for performing web service calls)*
* openSSL *(used by firm-dkim and base64 decoding)*
* libxml2 *(for parsing the DMARC reports)*
* zlib *(for unzipping report files)* 

TODO
----

* Add support for multipart messages.
* The server is currently only handling base64 encoded emails, should add proper MIME handling.
* DKIM (firm-dkim): Add support for SHA1 and canoninical simple.
* Add support for larger files.
* Needs more testing + unit tests.
* Port to xBSD.

