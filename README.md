Comfirm DMARC Report Server "Comdark"
===========================
*v 0.7*

Server for receiving **DMARC** reports, validate their integrity, and passing them to a web service as JSON.

This server is not ready for production use at this point!


Features
--------

* Written in C with performance in mind.
* Acts as an SMTP server.
* Validates DKIM signatures for data integrity.
* Passes the reports in "real-time" to a web service as JSON data.
* Persistent queueing of web service requests. I.e. if the web service is down then the reports will be sent as soon as it gets up. Also means that it can survive a restart.

Install
-------

First, install firm-dkim (extended version):
    $ make
    # make install
    
Now install the server:

    $ ./configure
    $ make
    # make install
    # comfirm-dmarc -c /etc/comfirm-dmarc/comfirm-dmarc.conf

TODO
----

* Add support for multipart messages.
* The server is currently only handling base64 encoded emails, should add proper MIME handling.
* DKIM (firm-dkim): Add support for SHA1 and canoninical simple
* Add support for larger files.
* Needs more testing + unit tests.
* Port to xBSD.

