#!/bin/sh

find . -iname '*.[ch]pp' \! -path '*/build/*' \! -path './.git/*' -exec clang-format -i {} \; 
