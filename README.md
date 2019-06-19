# delve - a simple terminal gopher client
Because Gopher should not die!

[![Build Status](https://travis-ci.org/kieselsteini/delve.svg?branch=master)](https://travis-ci.org/kieselsteini/delve)

## Overview
- configurable gopher selector handlers
- "powerful" shell
- bookmarks
- variables
- command aliases
- VT100 compatible with ANSI escape sequences
- no external dependencies
	- GNU readline is fully optional
- internal pager for text & menus
- less than *1k lines* of *C* code

## How to compile?
- clone this git repo
- just type `make` on any Unix compatible system (remove GNU readline if you don't have it)
	- currently tested on
		- macOS
		- Linux
		- OpenBSD 6.5
- type `make install` to install it on the system (defaults to /usr/local)

## How to contribute?
- send me pull-requests and I'll review and merge them :)
- if you wish to appear on the `help authors` command just add yourself there

## License
- [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html)

## Statistic
Language|files|blank|comment|code
:-------|-------:|-------:|-------:|-------:
C|1|185|33|919

## Help
Just type `help` when the client is running.
