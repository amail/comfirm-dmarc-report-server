Comfirm DMARC Report Server
===========================
v 0.7

Server for receiving DMARC reports and passing them to a web service as JSON.

Features
========

* Acts as an SMTP server, no need for additional software.
* High performance, uses epoll().
* Validates DKIM signatures for data integrity.
* Passes the reports in real-time to a web service as JSON.

