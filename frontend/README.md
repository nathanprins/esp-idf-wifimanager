# Generating an HTML Dump
An html dump is easily generated using the Linux tool `xdd`. All you have to do is run `xxd -i min.html > webapp.h` and you get a complete character array. This file can then be included in the source code and the contents can be served.
