Comfirm DMARC Report Server
===========================
*v 0.7*

Server for receiving **DMARC** reports, validate their integrity, and passing them to a web service as JSON.

This server is not ready for production use at this point!


Features
--------

* Acts as an SMTP server, no need for any 3rd-party software.
* High performance, uses epoll().
* Validates DKIM signatures for data integrity.
* Passes the reports in "real-time" to a web service as JSON data.
* Internal queueing - persistant.


Install
-------

    $ ./configure
    $ make
    # make install
    # comfirm-dmarc -c /etc/comfirm-dmarc/comfirm-dmarc.conf

TODO
----

* The server is currently only handling base64 encoded emails, should add proper MIME handling.
* Using an extended version of firm-dkim to validate signatures, only supports relaxed canon algorithm.
* Needs more testing.
