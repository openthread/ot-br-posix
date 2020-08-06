# http-parser

## URL

https://github.com/nodejs/http-parser/releases

## Version 

[2.9.4](https://github.com/nodejs/http-parser/releases/tag/v2.9.4)

## License

MIT License

## License File

[LICENSE](repo/LICENSE)

## Description

[Intro Page](https://github.com/nodejs/http-parser)

This is a parser for HTTP messages written in C. It parses both requests and responses. The parser is designed to be used in performance HTTP applications. It does not make any syscalls nor allocations, it does not buffer data, it can be interrupted at anytime. Depending on your architecture, it only requires about 40 bytes of data per message stream (in a web server that is per connection).
